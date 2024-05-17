This is a fork of the repository [TreeAnt](https://bitbucket.org/dav_gir/treeant/src/master/) a C++ implementation of the TREANT algorithm proposed by **Calzavara *et. al.*** in their research paper titled [Treant: training evasion-aware decision trees](https://dl.acm.org/doi/10.1007/s10618-020-00694-9).

To compile TREANT the project folder must include a folder named **3rdparty** with the required third party libraries:
(see FindNlopt.cmake).

```bash
├── 3rdparty
│   └── nlopt-*
│       ├── include
│       └── lib
```

The nlopt is a library used to solve the optimization problem.
Follow the guide at:
https://nlopt.readthedocs.io/en/latest/NLopt_Installation/
install in local folder using:

```bash
cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=$HOME/install ..
```

(see "Changing the installation directory" chapter).
After:
```bash
make install
```
the installation folder will contain the include and lib folder.
Tested using static libraries: add -DBUILD_SHARED_LIBS=OFF to cmake flags.


## Credits

```
@article{DBLP:journals/datamine/CalzavaraLTAO20,
  author       = {Stefano Calzavara and
                  Claudio Lucchese and
                  Gabriele Tolomei and
                  Seyum Assefa Abebe and
                  Salvatore Orlando},
  title        = {Treant: training evasion-aware decision trees},
  journal      = {Data Min. Knowl. Discov.},
  volume       = {34},
  number       = {5},
  pages        = {1390--1420},
  year         = {2020},
  url          = {https://doi.org/10.1007/s10618-020-00694-9},
  doi          = {10.1007/S10618-020-00694-9},
  timestamp    = {Tue, 13 Jul 2021 13:27:03 +0200},
  biburl       = {https://dblp.org/rec/journals/datamine/CalzavaraLTAO20.bib},
  bibsource    = {dblp computer science bibliography, https://dblp.org}
}
```
