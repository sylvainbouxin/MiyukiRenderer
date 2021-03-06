//
// Created by Shiina Miyuki on 2019/1/29.
//

#ifndef MIYUKI_RENDERSYSTEM_HPP
#define MIYUKI_RENDERSYSTEM_HPP

#include <sstream>
#include "../utils/util.h"
#include "../core/mesh.h"
#include "../core/scene.h"
#include "../integrators/integrator.h"


void saveAtExit();
namespace Miyuki {
    class RenderSystem {
        Scene scene;
        std::unique_ptr<Integrator> integrator;
        std::string integratorName;
        std::string outputFile;
        bool saved;
    public:
        RenderSystem() {
            saved = false;
            Init();
            outputFile = "out.png";
            std::atexit(saveAtExit);
        }

        void processOptions(int32_t argc, char **argv) {
            std::atexit(saveAtExit);
            printWelcome();
            if (argc <= 1) {
                exit(0);
            }
            readDescription(argv[1]);
        }

        void save() {
            if (integrator && !saved) {
                scene.writeImage(outputFile);
                fmt::print("Image saved to {}\n", outputFile);
                saved = true;
            }
        }

        int32_t render() {
            if (!integrator) { return -1; }
            fmt::print(R"(
Resolution: {0}x{1}
Output File: {2}
Samples per Pixel: {3}
Integrator: {4}
)", scene.getResolution().x(),
                       scene.getResolution().y(),
                       outputFile,
                       scene.option.samplesPerPixel,
                       integratorName);
            integrator->render(scene);
            save();
            return 0;
        }

        void readDescription(const std::string &filename);

        void printWelcome() {
            const char *welcome = R"(
        _             _    _     __                _
  /\/\ (_)_   _ _   _| | _(_)   /__\ ___ _ __   __| | ___ _ __ ___ _ __
 /    \| | | | | | | | |/ / |  / \/// _ \ '_ \ / _` |/ _ \ '__/ _ \ '__|
/ /\/\ \ | |_| | |_| |   <| | / _  \  __/ | | | (_| |  __/ | |  __/ |
\/    \/_|\__, |\__,_|_|\_\_| \/ \_/\___|_| |_|\__,_|\___|_|  \___|_|
          |___/

)";
            fmt::print("{}\n", welcome);
        }

        int32_t exec() {
            return render();
        }

        ~RenderSystem() {
            Exit();
        }
    };
}
extern Miyuki::RenderSystem renderSystem;
#endif //MIYUKI_RENDERSYSTEM_HPP
