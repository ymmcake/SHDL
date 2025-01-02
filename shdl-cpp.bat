@echo off
cpp -C -P -nostdinc %1 | .\shdl.exe > %2
