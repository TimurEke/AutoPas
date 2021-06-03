
#include "autopas/AutoPasImpl.h"
#include "autopas/molecularDynamics/LJFunctor.h"
#include "src/TypeDefinitions.h"

template bool autopas::AutoPas<ParticleType>::iteratePairwise(autopas::LJFunctor<ParticleType, true, true> *);
