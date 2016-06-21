#include "storm/settings/modules/JitBuilderSettings.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/SettingMemento.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Argument.h"

#include "storm/exceptions/InvalidSettingsException.h"

namespace storm {
    namespace settings {
        namespace modules {
            const std::string JitBuilderSettings::moduleName = "jitbuilder";
            
            const std::string JitBuilderSettings::doctorOptionName = "doctor";
            const std::string JitBuilderSettings::compilerOptionName = "compiler";
            const std::string JitBuilderSettings::stormRootOptionName = "storm";
            const std::string JitBuilderSettings::stormConfigDirectoryOptionName = "stormconf";
            const std::string JitBuilderSettings::boostIncludeDirectoryOptionName = "boost";
            const std::string JitBuilderSettings::carlIncludeDirectoryOptionName = "carl";
            const std::string JitBuilderSettings::compilerFlagsOptionName = "cxxflags";

            JitBuilderSettings::JitBuilderSettings() : ModuleSettings(moduleName) {
                this->addOption(storm::settings::OptionBuilder(moduleName, doctorOptionName, false, "Show debugging information on why the jit-based model builder is not working on your system.").build());
                this->addOption(storm::settings::OptionBuilder(moduleName, compilerOptionName, false, "The compiler in the jit-based model builder.")
                                .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the executable. Defaults to c++.").setDefaultValueString("c++").build()).build());
                this->addOption(storm::settings::OptionBuilder(moduleName, stormRootOptionName, false, "The root directory of Storm.")
                                .addArgument(storm::settings::ArgumentBuilder::createStringArgument("dir", "The directory that contains the src/ subtree of Storm.").build()).build());
                this->addOption(storm::settings::OptionBuilder(moduleName, stormConfigDirectoryOptionName, false, "The directory containing Storm's configuration (storm-config.h).")
                                .addArgument(storm::settings::ArgumentBuilder::createStringArgument("dir", "The directory that contains storm-config.h.").build()).build());
                this->addOption(storm::settings::OptionBuilder(moduleName, boostIncludeDirectoryOptionName, false, "The include directory of boost.")
                                .addArgument(storm::settings::ArgumentBuilder::createStringArgument("dir", "The directory containing the boost headers version >= 1.61.").build()).build());
                this->addOption(storm::settings::OptionBuilder(moduleName, carlIncludeDirectoryOptionName, false, "The include directory of carl.")
                                .addArgument(storm::settings::ArgumentBuilder::createStringArgument("dir", "The directory containing the carl headers.").build()).build());
                this->addOption(storm::settings::OptionBuilder(moduleName, compilerFlagsOptionName, false, "The flags passed to the compiler.")
                                .addArgument(storm::settings::ArgumentBuilder::createStringArgument("flags", "The compiler flags.").build()).build());
            }
            
            bool JitBuilderSettings::isCompilerSet() const {
                return this->getOption(compilerOptionName).getHasOptionBeenSet();
            }
            
            std::string JitBuilderSettings::getCompiler() const {
                return this->getOption(compilerOptionName).getArgumentByName("name").getValueAsString();
            }
            
            bool JitBuilderSettings::isStormRootSet() const {
                return this->getOption(stormRootOptionName).getHasOptionBeenSet();
            }
            
            std::string JitBuilderSettings::getStormRoot() const {
                return this->getOption(stormRootOptionName).getArgumentByName("dir").getValueAsString();
            }
            
            bool JitBuilderSettings::isStormConfigDirectorySet() const {
                return this->getOption(stormConfigDirectoryOptionName).getHasOptionBeenSet();
            }
            
            std::string JitBuilderSettings::getStormConfigDirectory() const {
                return this->getOption(stormConfigDirectoryOptionName).getArgumentByName("dir").getValueAsString();
            }
            
            bool JitBuilderSettings::isBoostIncludeDirectorySet() const {
                return this->getOption(boostIncludeDirectoryOptionName).getHasOptionBeenSet();
            }
            
            std::string JitBuilderSettings::getBoostIncludeDirectory() const {
                return this->getOption(boostIncludeDirectoryOptionName).getArgumentByName("dir").getValueAsString();
            }

            bool JitBuilderSettings::isCarlIncludeDirectorySet() const {
                return this->getOption(carlIncludeDirectoryOptionName).getHasOptionBeenSet();
            }
            
            std::string JitBuilderSettings::getCarlIncludeDirectory() const {
                return this->getOption(carlIncludeDirectoryOptionName).getArgumentByName("dir").getValueAsString();
            }

            bool JitBuilderSettings::isDoctorSet() const {
                return this->getOption(doctorOptionName).getHasOptionBeenSet();
            }
            
            bool JitBuilderSettings::isCompilerFlagsSet() const {
                return this->getOption(compilerFlagsOptionName).getHasOptionBeenSet();
            }
            
            std::string JitBuilderSettings::getCompilerFlags() const {
                return this->getOption(compilerFlagsOptionName).getArgumentByName("flags").getValueAsString();
            }
            
            void JitBuilderSettings::finalize() {
                // Intentionally left empty.
            }
            
            bool JitBuilderSettings::check() const {
                return true;
            }
        }
    }
}