#include "DftGspnSettings.h"

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

            const std::string DftGspnSettings::moduleName = "dftGspn";
            const std::string DftGspnSettings::transformToGspnOptionName = "to-gspn";
            const std::string DftGspnSettings::disableSmartTransformationOptionName = "disable-smart";
            const std::string DftGspnSettings::mergeDCFailedOptionName = "merge-dc-failed";
            const std::string DftGspnSettings::extendPrioritiesOptionName = "extend-priorities";


            DftGspnSettings::DftGspnSettings() : ModuleSettings(moduleName) {
                this->addOption(storm::settings::OptionBuilder(moduleName, transformToGspnOptionName, false, "Transform DFT to GSPN.").build());
                this->addOption(storm::settings::OptionBuilder(moduleName, disableSmartTransformationOptionName, false, "Disable smart transformation.").build());
                this->addOption(storm::settings::OptionBuilder(moduleName, mergeDCFailedOptionName, false, "Enable merging of Don't Care and Failed places into a combined place.").build());
                this->addOption(storm::settings::OptionBuilder(moduleName, extendPrioritiesOptionName, false,
                                                               "Enable experimental calculation of transition priorities").build());
            }

            bool DftGspnSettings::isTransformToGspn() const {
                return this->getOption(transformToGspnOptionName).getHasOptionBeenSet();
            }

            bool DftGspnSettings::isDisableSmartTransformation() const {
                return this->getOption(disableSmartTransformationOptionName).getHasOptionBeenSet();
            }

            bool DftGspnSettings::isMergeDCFailed() const {
                return this->getOption(mergeDCFailedOptionName).getHasOptionBeenSet();
            }

            bool DftGspnSettings::isExtendPriorities() const {
                return this->getOption(extendPrioritiesOptionName).getHasOptionBeenSet();
            }

            void DftGspnSettings::finalize() {
            }

            bool DftGspnSettings::check() const {
                // Ensure that GSPN option is set if other options are set.
                STORM_LOG_THROW(isTransformToGspn() || (!isDisableSmartTransformation() && !isMergeDCFailed()), storm::exceptions::InvalidSettingsException,
                                "GSPN transformation should be enabled when giving options for the transformation.");
                return true;
            }

        } // namespace modules
    } // namespace settings
} // namespace storm

