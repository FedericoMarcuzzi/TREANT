This is a fork of the repository [TreeAnt](https://bitbucket.org/dav_gir/treeant/src/master/) a C++ implementation of the TREANT algorithm proposed by [Calzavara _et al._](https://dl.acm.org/doi/10.1007/s10618-020-00694-9).

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
