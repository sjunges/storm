#include "src/parser/AutoParser.h"

#include <string>
#include <cctype>

#include "src/exceptions/WrongFileFormatException.h"
#include "src/models/AbstractModel.h"
#include "src/parser/DeterministicModelParser.h"
#include "src/parser/NonDeterministicModelParser.h"

namespace storm {
namespace parser {

AutoParser::AutoParser(std::string const & transitionSystemFile, std::string const & labelingFile,
	std::string const & stateRewardFile, std::string const & transitionRewardFile)
	: model(nullptr) {
	storm::models::ModelType type = this->analyzeHint(transitionSystemFile);

	if (type == storm::models::Unknown) {
		LOG4CPLUS_ERROR(logger, "Could not determine file type of " << transitionSystemFile << ".");
		LOG4CPLUS_ERROR(logger, "The first line of the file should contain a format hint. Please fix your file and try again.");
		throw storm::exceptions::WrongFileFormatException() << "Could not determine type of file " << transitionSystemFile;
	} else {
		LOG4CPLUS_INFO(logger, "Model type seems to be " << type);
	}

	// Do actual parsing
	switch (type) {
		case storm::models::DTMC: {
			DeterministicModelParser parser(transitionSystemFile, labelingFile, stateRewardFile, transitionRewardFile);
			this->model = parser.getDtmc();
			break;
		}
		case storm::models::CTMC: {
			DeterministicModelParser parser(transitionSystemFile, labelingFile, stateRewardFile, transitionRewardFile);
			this->model = parser.getCtmc();
			break;
		}
		case storm::models::MDP: {
			NonDeterministicModelParser parser(transitionSystemFile, labelingFile, stateRewardFile, transitionRewardFile);
			this->model = parser.getMdp();
			break;
		}
		case storm::models::CTMDP: {
			NonDeterministicModelParser parser(transitionSystemFile, labelingFile, stateRewardFile, transitionRewardFile);
			this->model = parser.getCtmdp();
			break;
		}
		default: ;  // Unknown
	}

	
	if (!this->model) {
		LOG4CPLUS_WARN(logger, "Model is still null.");
	}
}

storm::models::ModelType AutoParser::analyzeHint(const std::string& filename) {
	storm::models::ModelType hintType = storm::models::Unknown;
	// Open file
	MappedFile file(filename.c_str());
	char* buf = file.data;

	// parse hint
	char hint[128];
	sscanf(buf, "%s\n", hint);
	for (char* c = hint; *c != '\0'; c++) *c = toupper(*c);

	// check hint
	if (strncmp(hint, "DTMC", sizeof(hint)) == 0) hintType = storm::models::DTMC;
	else if (strncmp(hint, "CTMC", sizeof(hint)) == 0) hintType = storm::models::CTMC;
	else if (strncmp(hint, "MDP", sizeof(hint)) == 0) hintType = storm::models::MDP;
	else if (strncmp(hint, "CTMDP", sizeof(hint)) == 0) hintType = storm::models::CTMDP;

	return hintType;
}

}  // namespace parser
}  // namespace storm
