/*****************************************************************************

Copyright (c) 2023, 2024, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/


// Package ssot provides a single memSource of truth in transfer-test.

package ssot

import (
	"errors"
	"fmt"
	"sort"
	"sync"

	"go.uber.org/atomic"
)

type Record struct {
	Balance int
	Version int
}

type recordWithTs struct {
	Record

	ts int64
}

type account struct {
	lock sync.RWMutex

	records []recordWithTs
}

type Source interface {
	Transfer(src, dst, amount int, commitTs int64) error
	Query(id int, ts int64) (Record, error)
	GC(safeTs int64) error
}

type memSource struct {
	accounts []account

	lastGC atomic.Int64
}

func (s *memSource) Transfer(src, dst, amount int, commitTs int64) error {
	// Avoid deadlock
	if src > dst {
		src, dst = dst, src
		amount = -amount
	}

	s.accounts[src].lock.Lock()
	defer s.accounts[src].lock.Unlock()

	s.accounts[dst].lock.Lock()
	defer s.accounts[dst].lock.Unlock()

	var records *[]recordWithTs
	var oldRecord Record
	var ts int64

	records = &(s.accounts[src].records)
	oldRecord = (*records)[len(*records)-1].Record
	ts = (*records)[len(*records)-1].ts
	if ts >= commitTs {
		return fmt.Errorf("unexpected commitTs %v > %v", commitTs, ts)
	}
	*records = append(*records, recordWithTs{
		Record: Record{
			Balance: oldRecord.Balance - amount,
			Version: oldRecord.Version + 1,
		},
		ts: commitTs,
	})

	records = &(s.accounts[dst].records)
	oldRecord = (*records)[len(*records)-1].Record
	ts = (*records)[len(*records)-1].ts
	if ts >= commitTs {
		return fmt.Errorf("unexpected commitTs %v > %v", commitTs, ts)
	}
	*records = append(*records, recordWithTs{
		Record: Record{
			Balance: oldRecord.Balance + amount,
			Version: oldRecord.Version + 1,
		},
		ts: commitTs,
	})

	return nil
}

func (s *memSource) Query(id int, ts int64) (Record, error) {
	if ts < s.lastGC.Load() {
		return Record{}, errors.New("ts too old")
	}
	account := &s.accounts[id]

	account.lock.RLock()
	defer account.lock.RUnlock()

	records := account.records

	// Fast search
	i := 1
	for i <= len(records) && records[len(records)-i].ts > ts {
		i <<= 1
	}
	// assert left == 0 || records[left].ts <= ts
	// assert right == len(records) || records[right].ts > ts
	left := len(records) - i
	right := len(records) - (i >> 1)
	idx := sort.Search(right-left, func(i int) bool {
		j := i + left
		return records[j].ts > ts
	}) + left
	idx--

	return records[idx].Record, nil
}

func (s *memSource) GC(safeTs int64) error {
	// STW
	for i := range s.accounts {
		account := &s.accounts[i]
		account.lock.Lock()
	}

	defer func() {
		for i := range s.accounts {
			account := &s.accounts[i]
			account.lock.Unlock()
		}
	}()

	for i := range s.accounts {
		account := &s.accounts[i]
		records := account.records
		idx := sort.Search(len(records), func(j int) bool {
			return records[j].ts > safeTs
		})
		if idx != 0 {
			idx--
		}
		account.records = append([]recordWithTs(nil), records[idx:]...)
	}

	s.lastGC.Store(safeTs)

	return nil
}

func NewMemSsot(records []Record) *memSource {
	s := &memSource{
		accounts: make([]account, len(records)),
	}
	for i, record := range records {
		s.accounts[i].records = []recordWithTs{{
			ts:     0,
			Record: record,
		}}
	}
	return s
}

var _ Source = &memSource{}
