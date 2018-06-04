# AutoPas
AutoPas is a node-level auto-tuned particle simulation library developed
in the context of the **TaLPas** project. [![Build Status](https://www5.in.tum.de/jenkins/mardyn/buildStatus/icon?job=AutoPas-Multibranch/master)](https://www5.in.tum.de/jenkins/mardyn/job/AutoPas-Multibranch/job/master/)

## Documentation
The documentation can be found at our website:
 https://www5.in.tum.de/AutoPas/doxygen_doc/master/

Alternatively you can build the documentation on your own:
* requirements:
 doxygen
* `make doc_doxygen`


## Requirements
* cmake 3.3 or newer
* make (build-essentials) or ninja
* a c++11 compiler (gcc7 and clang6 are tested)


## Building AutoPas
build instructions for make:
```
mkdir build
cd build
cmake ..
make
```
if you want to use another compiler, specify it before the first cmake call, e.g.:
```
mkdir build
cd build
CC=clang CXX=clang++ cmake ..
make
```
if you would like to use ninja instead of make:
```
mkdir build
cd build
cmake -G Ninja ..
ninja
```

## Testing
to run tests:
```
make test
// or
ninja test
```
or using the ctest environment:
```
ctest
```
to get verbose output:
```
ctest --verbose
```
* to run specific tests:
use the --gtest_filter variable:
```
./tests/testAutopas/runTests --gtest_filter=ArrayMathTest.testAdd*
```
or use the GTEST_FILTER environment variable:
```
GTEST_FILTER="ArrayMathTest.testAdd*" ctest --verbose
```


## Examples
As AutoPas is only a library for particle simulations it itself is not able to run simulations.
We have, however, included a variety of examples in the **examples** directory. The examples include:
* Molecular dynamics simulations with 1 centered Lennard-Jones particles.
* Smoothed particle hydrodynamics simulations
* Gravity simulations


## Using AutoPas

Steps to using AutoPas in your particle simulation program:
### Defining a Custom Particle Class
First you will need to define a particle class.
For that we provide some basic Particle classes defined
in `src/particles/` that you can use either directly
or you can write your own Particle class by inheriting from
one of the provided classes.
```
class SPHParticle : public autopas::Particle {

}
```
### AutoPas and Containers
creating your container, adding particles,
### Functors
Once you have defined your particle you can start with functors;
#### Definition
#### Usage

### Iterating Through Particles
Iterators to iterate over particle are provided.
The particle can be accesses using `iter->` (`*iter` is also possible), e.g.
```
for(auto iter = container.begin(), iter.isValid(); ++iter){
    // user code
    auto position = iter->getR();
}
```

### Updating the Container
#### How
You can update the container using
```
ParticleContainer::updateContainer()
```
#### When it is necessary
You have to update the container when the two conditions are fullfilled:
* If you moved particles
* You want to use `iteratePairwise()` or a RegionParticleIterator


#### When it is not enough
If you moved particles by more than one interaction length.
If you are planning to move particles by a long distance,
e.g. because of boundary conditions please delete the particles and add them again:
```
std::vector<autopas::sph::SPHParticle> invalidParticles;
for (auto part = sphSystem.begin(); part.isValid(); ++part) {
  if( /*check*/){
    invalidParticles.push_back(*part);
    part.deleteCurrentParticle();
  }
}
for (auto p: invalidParticles) {
  sphSystem.addParticle(p);
}
```

#### Special exceptions
* Verlet-Lists, here it is safe to not update the container
as long as particles move not more than a skin radius.


## Developing AutoPas
* We use the google code style with minor modifications (see .clang-format)
* code style can be build with `make clangformat`
* requirements:
	clang-format-6.0 
	(Version is important since formatting is not consistent)

## Acknowledgements
* TaLPas BMBF

## Papers to cite
* TODO: Add papers
