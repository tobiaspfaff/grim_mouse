#!/bin/bash
tooldir=../residualvm-tools
grimex=~/grim/ex_lua
tmpdir=/tmp/grim_pnc
delta=dists/engine-data/patches/grim_pnc_delta
lab=dists/engine-data/mouse.lab

mkdir -p $tmpdir
rm -rf $tmpdir/*
$tooldir/luac -o $delta/mouse.lua $grimex/mouse.lua
cp $delta/* $tmpdir/
python script/convert_to_bin.py
python script/patch_lua.py
$tooldir/mklab --grim $tmpdir $lab
cp $lab .
