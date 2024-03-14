#include "IRModule.h"
#include "IRType.h"
#include <fstream>

namespace ola
{

	IRModule::IRModule(IRContext& context, std::string_view module_id) : context(context), module_id(module_id)
	{
	}

	IRModule::~IRModule()
	{
	}

	
	void IRModule::PrintIR(std::string_view filename)
	{
		std::ofstream ola_ir_stream(filename.data());
	}

}

