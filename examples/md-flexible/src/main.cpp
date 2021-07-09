/**
 * @file main.cpp
 * @date 23.02.2018
 * @author F. Gratl
 */

#include <fstream>

#include "Simulation.h"
#include "autopas/utils/WrapMPI.h"

// Declare the main AutoPas class as extern template instantiation. It is instantiated in AutoPasClass.cpp.
extern template class autopas::AutoPas<ParticleType>;

/**
 * The main function for md-flexible.
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char **argv) {
  autopas::AutoPas_MPI_Init(&argc, &argv);

  MDFlexConfig configuration(argc, argv);
  std::cout << configuration.to_string() << std::endl;

  RegularGridDecomposition domainDecomposition(configuration.boxMin.value, configuration.boxMax.value,
                                               configuration.cutoff.value, configuration.verletSkinRadius.value);

  if (domainDecomposition.getDomainIndex() == 0) {
    std::cout << std::endl << "Using " << autopas::autopas_get_max_threads() << " Threads" << std::endl;
  }

  Simulation simulation(configuration, domainDecomposition);
  simulation.run();

  if (domainDecomposition.getDomainIndex() == 0) {
    std::cout << std::endl << "Using " << autopas::autopas_get_max_threads() << " Threads" << std::endl;

    if (configuration.dontCreateEndConfig.value) {
      std::ofstream configFileEnd("MDFlex_end_" + autopas::utils::Timer::getDateStamp() + ".yaml");
      if (configFileEnd.is_open()) {
        configFileEnd << "# Generated by:" << std::endl;
        configFileEnd << "# ";
        for (int i = 0; i < argc; ++i) {
          configFileEnd << argv[i] << " ";
        }
        configFileEnd << std::endl;
        configFileEnd << configuration;
        configFileEnd.close();
      }
    }
  }

  autopas::AutoPas_MPI_Finalize();
  return EXIT_SUCCESS;
}
