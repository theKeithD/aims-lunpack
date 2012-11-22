lunpack
=======
Unpacker utility for games created using the [AIMS 2D game engine](http://aims.dna-softwares.com/?page_id=14 "(Japanese)") by D.N.A.Softwares.

Usage
-----
`lunpack packfile.p [-d]`

`-d` will attempt to decompress files in the packfile using `LLZSS.exe`.

Outputs files into `packfile/` (or `packfile-music/` if processing a `.mus` file).

Notes
-----
`LLZSS.exe` and `LPACK.exe` from the AIMS 1.8.0 toolkit are included for convenience.
The copy of `lunpack.exe` in this repository was compiled on Win7 x64 using MinGW.