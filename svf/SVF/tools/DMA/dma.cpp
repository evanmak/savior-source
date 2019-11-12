//===- dma.cpp -- Dominator Analysis -------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yueqi Chen>
// 

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===-----------------------------------------------------------------------===//

/*
 // Dominator Analysis
 //
 // Author: Yueqi Chen,
 // Data: Nov 16 2017
 */

#include "DMA/DMAPass.h"
#include <llvm-c/Core.h> // for LLVMGetGlobalContext()
#include <llvm/Support/CommandLine.h>	// for cl
#include <llvm/Support/FileSystem.h>	// for sys::fs::F_None
#include <llvm/Bitcode/BitcodeWriterPass.h>  // for bitcode write
#include <llvm/IR/LegacyPassManager.h>		// pass manager
#include <llvm/Support/Signals.h>	// singal for command line
#include <llvm/IRReader/IRReader.h>	// IR reader for bit file
#include <llvm/Support/ToolOutputFile.h> // for tool output file
#include <llvm/Support/PrettyStackTrace.h> // for pass list
#include <llvm/IR/LLVMContext.h>		// for llvm LLVMContext
#include <llvm/Support/SourceMgr.h> // for SMDiagnostic
#include <llvm/Bitcode/BitcodeWriterPass.h>		// for createBitcodeWriterPass

using namespace llvm;

static cl::opt<std::string> InputFilename(cl::Positional,
        cl::desc("<input bitcode>"), cl::init("-"));


int main(int argc, char ** argv) {

    sys::PrintStackTraceOnErrorSignal(argv[0]); // provided in llvm/Support/Signals.h
    llvm::PrettyStackTraceProgram X(argc, argv); // provided in llvm/Support/PrettyStackTrace.h, prints a specified program arguments to the stream as the stack trace when a crash occurs.

    LLVMOpaqueContext * WrappedContextRef = LLVMGetGlobalContext(); //  provided in llvm-c/Core.h, obtain the global context instance. 
    LLVMContext &Context = *unwrap(WrappedContextRef); // an inline function

    std::string OutputFilename;

    cl::ParseCommandLineOptions(argc, argv, "Whole Program Points-to Analysis\n"); // provied in llvm/Support/CommandLine.h, designed to be called directly from main, used to fill in the values of all of the command line option
   // sys::PrintStackTraceOnErrorSignal(argv[0]); // what's this, why again?

    PassRegistry &Registry = *PassRegistry::getPassRegistry(); // access the global registry object, which is automatically initialized at application launch and destroyed by llvm_shutdown

    initializeCore(Registry);
    initializeScalarOpts(Registry);
    initializeIPO(Registry);
    initializeAnalysis(Registry);
    initializeTransformUtils(Registry);
    initializeInstCombine(Registry);
    initializeInstrumentation(Registry);
    initializeTarget(Registry);

    llvm::legacy::PassManager Passes;

    SMDiagnostic Err;

    // Load the input module...
    std::unique_ptr<Module> M1 = parseIRFile(InputFilename, Err, Context); // if the given file holds a bitcode image, return a Module for it.

    if (!M1) {
        Err.print(argv[0], errs());
        return 1;
    }


    std::unique_ptr<tool_output_file> Out;
    std::error_code ErrorInfo;

    StringRef str(InputFilename);
    InputFilename = str.rsplit('.').first;
    OutputFilename = InputFilename + ".dma";

    Out.reset(
        new tool_output_file(OutputFilename.c_str(), ErrorInfo,
                             sys::fs::F_None));

    if (ErrorInfo) {
        errs() << ErrorInfo.message() << '\n';
        return 1;
    }

    Passes.add(new DMAPass()); // add a pass to the queue of passes to run 
		// Passes.add(Pass *p) -> schedulePass(Pass* p) 
		// schedule pass P for execution, make sure that passes required by P are run before P is run

    Passes.add(createBitcodeWriterPass(Out->os())); // create and return a pass that writes the module to the specified ostream

    Passes.run(*M1.get()); // execute all the passes scheduled for execution, keep track of whether any of the passes modified the module, if so, return true
		// execute all of the passes scheduled for execution by invoking runOnModule method. 
    Out->keep();

    return 0;

}

