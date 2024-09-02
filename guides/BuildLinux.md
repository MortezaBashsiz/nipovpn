## Build on Linux

### Requirements

    C++20 or higher
    libyaml-cpp >= 0.8
    boost-libs >= 1.75.0
    OpenSSL

### Dependencies
Install the dependencies
```bash
sudo apt install cmake libssl-dev
```
#### Install Boost Library:
Follow the instructions on the [Boost](https://www.boost.org/) to install the Boost library suitable for your system.
or install from repository
```bash
sudo apt install libboost-all-dev
```

#### Install Yaml-cpp Library:
Follow the instructions on the [Yaml-cpp](https://github.com/jbeder/yaml-cpp) to install the library

### Build with make (Recommended)
    - git clone git@github.com:MortezaBashsiz/nipovpn.git
    - cd niopvpn
    - mkdir build
    - cd build
    - cmake -DCMAKE_BUILD_TYPE=Debug ..
    - make -j$(nproc)

### Build with Ninja
    - sudo apt install ninja-build
    - git clone git@github.com:MortezaBashsiz/nipovpn.git
    - cd niopvpn
    - cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja -B build
    - cmake --build build -j$(nproc)

## Cmake options

### Build type

    -DCMAKE_BUILD_TYPE=Debug|Release

#### Clean

To clean simply remove the build directory


