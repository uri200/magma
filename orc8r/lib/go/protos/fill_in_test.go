/*
Copyright (c) Facebook, Inc. and its affiliates.
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

package protos_test

import (
	"reflect"
	"testing"

	"magma/orc8r/lib/go/protos"

	"github.com/stretchr/testify/assert"
)

func TestOverlapFillIn(t *testing.T) {

	type Embeded1 struct {
		a, B, C int
	}
	type Embeded2 struct {
		C, d, E int
	}

	type S_A struct {
		S          string
		I          int
		E          Embeded1
		hidden     bool
		S1         string
		MixEdCaSe  string
		mixEdCaSe1 string
		MixEdCaSe2 string
		SL1        []Embeded1
		SL2        []*Embeded2
		SL3		   []*Embeded1
		M          map[string]Embeded1
		M1         map[string]*Embeded1
		M2         map[string]Embeded1
		MSL3	   map[string][]Embeded1
		MSL4       map[string][]*Embeded2
	}

	type S_B struct {
		S1         string
		I          int
		E          Embeded2
		hidden     bool
		MiXedCase  string
		MiXedCase1 string
		miXedCase2 string
		SL1        []Embeded1
		SL2        []*Embeded2
		SL3		   []*Embeded1
		M          map[string]*Embeded2
		M1         map[string]*Embeded1
		M2         map[string]Embeded1
		MSL3	   map[string][]Embeded1
		MSL4       map[string][]*Embeded2
	}

	type TestInitStr1 struct {
		I1      int
		F1      float32
		S11     string
		hidden1 bool
		E1      Embeded1
		EPtr1   *Embeded2
		ISlice1 []int
		IArr1   [5]int
	}

	type TestInitStr2 struct {
		I2      int
		S2      string
		hidden2 bool
		E2      Embeded1
		EPtr2   *Embeded2
		TSPtr2  *TestInitStr1
		ISlice2 []int
		IArr2   [5]int
		M2      map[string]int
	}

	a := S_A{"str1", 1, Embeded1{1, 2, 3}, true, "str2",
		"bla Bla bla", "bla1", "bla2",
		[]Embeded1{},
		[]*Embeded2{{7, 8, 9} },
		nil,
		map[string]Embeded1{},
		map[string]*Embeded1{},
		nil,
		map[string][]Embeded1{"key1": {{4, 5, 6}}},
		map[string][]*Embeded2{"key1": {{4, 5, 6}}},
	}

	b := S_B{ "str3", 11, Embeded2{111, 222, 333},  false,
		"Foo bar", "foo1", "foo2",
		[]Embeded1{{1, 2, 3}, {4, 5, 6}},
		[]*Embeded2{ {1, 2, 3} , {4, 5, 6}},
		[]*Embeded1{ {1, 2, 3} , {4, 5, 6}},
		map[string]*Embeded2{"key": &Embeded2{1, 2, 3}},
		map[string]*Embeded1{"key": &Embeded1{1, 2, 3}},
		map[string]Embeded1{"key": Embeded1{1, 2, 3}},
		map[string][]Embeded1{"key1": {{1, 2, 3}}},
		map[string][]*Embeded2{"key2": {{1, 2, 3}}},
	}


	//src := []*Embeded1{{1, 2, 3}}
	//dst := []*Embeded1{}

	src := map[string]int{"key1": 1}
	dst := map[string]int{}

	protos.FillIn(src, dst)

	count := protos.FillIn(b, a)
	if count <= 0 || a.I != b.I || a.S1 != b.S1 || a.S != "str1" || b.I != 11 ||
		a.hidden == b.hidden || a.E.C != b.E.C || a.E.a != 1 || a.E.B != 2 ||
		a.MixEdCaSe != b.MiXedCase || a.mixEdCaSe1 == b.MiXedCase1 || a.MixEdCaSe2 == b.miXedCase2 ||
		len(a.M) != 1 || a.M["key"].C != 1 ||
		len(a.M1) != 1 || a.M1["key"].a != 1 || a.M1["key"].B != 2 || a.M1["key"].C != 3 ||
		len(a.M2) != 1 || a.M2["key"].a != 1 || a.M2["key"].B != 2 || a.M2["key"].C != 3 {

		t.Fatalf("Invalid assignment:\n\tcount: %d\n\ta: %+#v\n\tb: %+#v\n",
			count, a, b)
	}

	// slice checks
	if !reflect.DeepEqual(a.SL1, []Embeded1{ {1, 2, 3}, {4, 5, 6}} ) {
		t.Fatalf("Invalid assignment on slices 1:\n\tcount: %d\n\ta: %+#v\n\tb: %+#v\n",
			count, a, b)
	}
	if !reflect.DeepEqual(a.SL2, []*Embeded2{ {1, 2, 3} , {4, 5, 6}}) {
		t.Fatalf("Invalid assignment checking slices 2 (pointer):\n\tcount: %d\n\ta: %+#v\n\tb: %+#v\n",
			count, a, b)
	}
	if !reflect.DeepEqual(a.SL3, []*Embeded1{ {1, 2, 3} , {4, 5, 6}}) 	{
		t.Fatalf("Invalid assignment checking slices 2 (dst nil):\n\tcount: %d\n\ta: %+#v\n\tb: %+#v\n",
			count, a, b)
	}
	if !reflect.DeepEqual(a.MSL3, map[string][]Embeded1{"key1": {{1, 2, 3}}}) {
		t.Fatalf("Invalid assignment checking map of slices 1:\n\tcount: %d\n\ta: %+#v\n\tb: %+#v\n",
			count, a, b)
	}
	if !reflect.DeepEqual(a.MSL4, map[string][]*Embeded2{"key2": {{1, 2, 3}}}) {
		t.Fatalf("Invalid assignment checking map of slices 2 (pointer):\n\tcount: %d\n\ta: %+#v\n\tb: %+#v\n",
			count, a, b)
	}

	tp := &TestInitStr2{}
	assert.Nil(t, tp.EPtr2)
	if tp.M2 != nil {
		t.Fatal("M2 != nil")
	}
	if tp.ISlice2 != nil {
		t.Fatal("ISlice2 != nil")
	}
	assert.Nil(t, tp.TSPtr2)
	assert.Nil(t, tp.EPtr2)

	tp = protos.SafeInit(tp).(*TestInitStr2)
	assert.NotNil(t, tp.EPtr2)
	if tp.M2 == nil {
		t.Fatal("M2 == nil")
	}
	if tp.ISlice2 == nil {
		t.Fatal("ISlice2 == nil")
	}
	assert.NotNil(t, tp.TSPtr2)
	assert.NotNil(t, tp.EPtr2)
	assert.NotNil(t, tp.TSPtr2.EPtr1)
	if tp.TSPtr2.ISlice1 == nil {
		t.Fatal("TSPtr2.ISlice1 == nil")
	}

	tp = nil
	tp = protos.SafeInit(tp).(*TestInitStr2)
	assert.NotNil(t, tp.EPtr2)
	if tp.M2 == nil {
		t.Fatal("M2 == nil")
	}
	if tp.ISlice2 == nil {
		t.Fatal("ISlice2 == nil")
	}
	assert.NotNil(t, tp.TSPtr2)
	assert.NotNil(t, tp.EPtr2)
	assert.NotNil(t, tp.TSPtr2.EPtr1)
	if tp.TSPtr2.ISlice1 == nil {
		t.Fatal("TSPtr2.ISlice1 == nil")
	}
}
