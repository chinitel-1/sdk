// Copyright (c) 2016, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "vm/compiler.h"
#include "vm/dart_api_impl.h"
#include "vm/dart_entry.h"
#include "vm/flow_graph_builder.h"
#include "vm/intermediate_language.h"
#include "vm/unit_test.h"

namespace dart {

#define DUMP_ASSERT(condition)                                                 \
  if (!(condition)) {                                                          \
    dart::Expect(__FILE__, __LINE__).Fail("expected: %s", #condition);         \
    THR_Print(">>> BEGIN source position table for `%s`\n", graph_name_);      \
    Dump();                                                                    \
    THR_Print("<<< END source position table for `%s`\n", graph_name_);        \
    OS::Abort();                                                               \
  }

class SourcePositionTest : public ValueObject {
 public:
  SourcePositionTest(Thread* thread,
                     const char* script)
      : thread_(thread),
        isolate_(thread->isolate()),
        script_(script),
        root_lib_(Library::Handle()),
        root_script_(Script::Handle()),
        graph_(NULL),
        blocks_(NULL) {
    EXPECT(thread_ != NULL);
    EXPECT(isolate_ != NULL);
    EXPECT(script_ != NULL);
    Dart_Handle lib = TestCase::LoadTestScript(script, NULL);
    EXPECT_VALID(lib);
    root_lib_ ^= Api::UnwrapHandle(lib);
    EXPECT(!root_lib_.IsNull());
    root_script_ ^= root_lib_.LookupScript(
        String::Handle(String::New(USER_TEST_URI)));
    EXPECT(!root_script_.IsNull());
  }

  void BuildGraphFor(const char* function_name) {
    graph_ = NULL;
    blocks_ = NULL;
    graph_name_ = NULL;

    // Only support unoptimized code for now.
    const bool optimized = false;

    const Function& function =
        Function::Handle(GetFunction(root_lib_, function_name));
    ZoneGrowableArray<const ICData*>* ic_data_array =
        new ZoneGrowableArray<const ICData*>();
    ParsedFunction* parsed_function = new ParsedFunction(
        thread_, Function::ZoneHandle(function.raw()));
    Parser::ParseFunction(parsed_function);
    parsed_function->AllocateVariables();
    FlowGraphBuilder builder(
        *parsed_function,
        *ic_data_array,
        NULL,
        Compiler::kNoOSRDeoptId);
    graph_ = builder.BuildGraph();
    EXPECT(graph_ != NULL);
    blocks_ = graph_->CodegenBlockOrder(optimized);
    EXPECT(blocks_ != NULL);
    graph_name_ = function_name;
    EXPECT(graph_name_ != NULL);
  }

  // Expect to find an instance call at |line| and |column|.
  void InstanceCallAt(intptr_t line,
                      intptr_t column = -1,
                      Token::Kind kind = Token::kNumTokens) {
    ZoneGrowableArray<Instruction*>* instructions =
        FindInstructionsAt(line, column);
    intptr_t count = 0;
    for (intptr_t i = 0; i < instructions->length(); i++) {
      Instruction* instr = instructions->At(i);
      EXPECT(instr != NULL);
      if (instr->IsInstanceCall()) {
        if (kind != Token::kNumTokens) {
          if (instr->AsInstanceCall()->token_kind() == kind) {
            count++;
          }
        } else {
          count++;
        }
      }
    }
    DUMP_ASSERT(count > 0);
  }

  // Expect to find at least one static call at |line| and |column|. The
  // static call will have |needle| in its |ToCString| representation.
  void StaticCallAt(const char* needle,
                    intptr_t line,
                    intptr_t column = -1) {
    ZoneGrowableArray<Instruction*>* instructions =
        FindInstructionsAt(line, column);
    intptr_t count = 0;
    for (intptr_t i = 0; i < instructions->length(); i++) {
      Instruction* instr = instructions->At(i);
      EXPECT(instr != NULL);
      if (instr->IsStaticCall()) {
        const char* haystack = instr->ToCString();
        if (strstr(haystack, needle) != NULL) {
          count++;
        }
      }
    }
    DUMP_ASSERT(count > 0);
  }

  // Expect that at least one of the instructions found at |line| and |column|
  // contain |needle| in their |ToCString| representation.
  void FuzzyInstructionMatchAt(const char* needle,
                               intptr_t line,
                               intptr_t column = -1) {
    ZoneGrowableArray<Instruction*>* instructions =
        FindInstructionsAt(line, column);
    intptr_t count = 0;
    for (intptr_t i = 0; i < instructions->length(); i++) {
      Instruction* instr = instructions->At(i);
      const char* haystack = instr->ToCString();
      if (strstr(haystack, needle) != NULL) {
        count++;
      }
    }
    DUMP_ASSERT(count > 0);
  }

  // Utility to dump the instructions with token positions or line numbers.
  void Dump() {
    for (intptr_t i = 0; i < blocks_->length(); i++) {
      BlockEntryInstr* entry = (*blocks_)[i];
      THR_Print("B%" Pd ":\n", entry->block_id());
      DumpInstruction(entry);
      for (ForwardInstructionIterator it(entry); !it.Done(); it.Advance()) {
        DumpInstruction(it.Current());
      }
    }
  }

 private:
  void DumpInstruction(Instruction* instr) {
    const intptr_t token_pos = instr->token_pos();
    if (token_pos < 0) {
      const char* token_pos_string =
          ClassifyingTokenPositions::ToCString(token_pos);
      THR_Print("%12s -- %s\n", token_pos_string, instr->ToCString());
      return;
    }
    intptr_t token_line = -1;
    intptr_t token_column = -1;
    root_script_.GetTokenLocation(token_pos,
                                  &token_line,
                                  &token_column,
                                  NULL);
    THR_Print("       %02d:%02d -- %s\n",
              static_cast<int>(token_line),
              static_cast<int>(token_column),
              instr->ToCString());
  }

  Instruction* FindFirstInstructionAt(intptr_t line, intptr_t column) {
    ZoneGrowableArray<Instruction*>* instructions =
        FindInstructionsAt(line, column);
    if (instructions->length() == 0) {
      return NULL;
    }
    return instructions->At(0);
  }

  ZoneGrowableArray<Instruction*>* FindInstructionsAt(
      intptr_t line, intptr_t column) {
    ZoneGrowableArray<Instruction*>* instructions =
        new ZoneGrowableArray<Instruction*>();
    for (intptr_t i = 0; i < blocks_->length(); i++) {
      BlockEntryInstr* entry = (*blocks_)[i];
      for (ForwardInstructionIterator it(entry); !it.Done(); it.Advance()) {
        Instruction* instr = it.Current();
        intptr_t token_pos = instr->token_pos();
        if (token_pos < 0) {
          continue;
        }
        intptr_t token_line = -1;
        intptr_t token_column = -1;
        root_script_.GetTokenLocation(token_pos,
                                      &token_line,
                                      &token_column,
                                      NULL);
        if (token_line == line) {
          if ((column < 0) || (column == token_column)) {
            instructions->Add(instr);
          }
        }
      }
    }
    return instructions;
  }

  ZoneGrowableArray<Instruction*>* FindInstructionsAt(intptr_t token_pos) {
    ZoneGrowableArray<Instruction*>* instructions =
        new ZoneGrowableArray<Instruction*>();
    for (intptr_t i = 0; i < blocks_->length(); i++) {
      BlockEntryInstr* entry = (*blocks_)[i];
      for (ForwardInstructionIterator it(entry); !it.Done(); it.Advance()) {
        Instruction* instr = it.Current();
        if (instr->token_pos() == token_pos) {
          instructions->Add(instr);
        }
      }
    }
    return instructions;
  }

  RawFunction* GetFunction(const Library& lib, const char* name) {
    const Function& result = Function::Handle(lib.LookupFunctionAllowPrivate(
        String::Handle(String::New(name))));
    EXPECT(!result.IsNull());
    return result.raw();
  }

  RawFunction* GetFunction(const Class& cls, const char* name) {
    const Function& result = Function::Handle(cls.LookupFunctionAllowPrivate(
        String::Handle(String::New(name))));
    EXPECT(!result.IsNull());
    return result.raw();
  }

  RawClass* GetClass(const Library& lib, const char* name) {
    const Class& cls = Class::Handle(
        lib.LookupClass(String::Handle(Symbols::New(name))));
    EXPECT(!cls.IsNull());  // No ambiguity error expected.
    return cls.raw();
  }

  Thread* thread_;
  Isolate* isolate_;
  const char* script_;
  Library& root_lib_;
  Script& root_script_;
  const char* graph_name_;
  FlowGraph* graph_;
  GrowableArray<BlockEntryInstr*>* blocks_;
};


TEST_CASE(SourcePosition_InstanceCalls) {
  const char* kScript =
      "var x = 5;\n"
      "var y = 5;\n"
      "main() {\n"
      "  var z = x + y;\n"
      "  return z;\n"
      "}\n";

  SourcePositionTest spt(thread, kScript);
  spt.BuildGraphFor("main");
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 3, 5);
  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 3, 5);
  spt.InstanceCallAt(4, 13, Token::kADD);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 5, 3);
  spt.FuzzyInstructionMatchAt("Return", 5, 3);
}


TEST_CASE(SourcePosition_If) {
  const char* kScript =
      "var x = 5;\n"
      "var y = 5;\n"
      "main() {\n"
      "  if (x != 0) {\n"
      "    return x;\n"
      "  }\n"
      "  return y;\n"
      "}\n";

  SourcePositionTest spt(thread, kScript);
  spt.BuildGraphFor("main");
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 3, 5);
  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 3, 5);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 4, 7);
  spt.InstanceCallAt(4, 9, Token::kEQ);
  spt.FuzzyInstructionMatchAt("Branch if StrictCompare", 4, 9);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 5, 12);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 5, 5);
  spt.FuzzyInstructionMatchAt("Return", 5, 5);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 7, 10);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 7, 3);
  spt.FuzzyInstructionMatchAt("Return", 7, 3);
}


TEST_CASE(SourcePosition_ForLoop) {
  const char* kScript =
      "var x = 0;\n"
      "var y = 5;\n"
      "main() {\n"
      "  for (var i = 0; i < 10; i++) {\n"
      "    x += i;\n"
      "  }\n"
      "  return x;\n"
      "}\n";

  SourcePositionTest spt(thread, kScript);
  spt.BuildGraphFor("main");
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 3, 5);
  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 3, 5);
  spt.FuzzyInstructionMatchAt("StoreLocal", 4, 14);
  spt.FuzzyInstructionMatchAt("LoadLocal", 4, 19);
  spt.InstanceCallAt(4, 21, Token::kLT);
  spt.FuzzyInstructionMatchAt("Branch if StrictCompare", 4, 21);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 5, 5);
  spt.FuzzyInstructionMatchAt("StoreStaticField", 5, 5);
  spt.InstanceCallAt(5, 7, Token::kADD);
  spt.FuzzyInstructionMatchAt("LoadLocal", 5, 10);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 7, 10);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 7, 3);
  spt.FuzzyInstructionMatchAt("Return", 7, 3);
}


TEST_CASE(SourcePosition_While) {
  const char* kScript =
      "var x = 0;\n"
      "var y = 5;\n"
      "main() {\n"
      "  while (x < 10) {\n"
      "    if (y == 5) {\n"
      "      return y;\n"
      "    }\n"
      "    x++;\n"
      "  }\n"
      "  return x;\n"
      "}\n";

  SourcePositionTest spt(thread, kScript);
  spt.BuildGraphFor("main");
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 3, 5);
  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 3, 5);

  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 4, 3);
  spt.FuzzyInstructionMatchAt("Constant", 4, 10);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 4, 10);
  spt.InstanceCallAt(4, 12, Token::kLT);
  spt.FuzzyInstructionMatchAt("Branch if StrictCompare", 4, 12);

  spt.FuzzyInstructionMatchAt("Constant", 5, 9);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 5, 9);
  spt.InstanceCallAt(5, 11, Token::kEQ);
  spt.FuzzyInstructionMatchAt("Branch if StrictCompare", 5, 11);

  spt.FuzzyInstructionMatchAt("Constant", 6, 14);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 6, 14);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 6, 7);
  spt.FuzzyInstructionMatchAt("Return", 6, 7);

  spt.FuzzyInstructionMatchAt("Constant", 8, 5);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 8, 5);
  spt.FuzzyInstructionMatchAt("Constant(#1)", 8, 6);
  spt.InstanceCallAt(8, 6, Token::kADD);
  spt.FuzzyInstructionMatchAt("StoreStaticField", 8, 5);

  spt.FuzzyInstructionMatchAt("LoadStaticField", 10, 10);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 10, 3);
  spt.FuzzyInstructionMatchAt("Return", 10, 3);
}


TEST_CASE(SourcePosition_WhileContinueBreak) {
  const char* kScript =
      "var x = 0;\n"
      "var y = 5;\n"
      "main() {\n"
      "  while (x < 10) {\n"
      "    if (y == 5) {\n"
      "      continue;\n"
      "    }\n"
      "    break;\n"
      "  }\n"
      "  return x;\n"
      "}\n";

  SourcePositionTest spt(thread, kScript);
  spt.BuildGraphFor("main");
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 3, 5);
  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 3, 5);

  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 4, 3);
  spt.FuzzyInstructionMatchAt("Constant(#Field", 4, 10);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 4, 10);
  spt.FuzzyInstructionMatchAt("Constant(#10", 4, 14);
  spt.InstanceCallAt(4, 12, Token::kLT);
  spt.FuzzyInstructionMatchAt("Branch if StrictCompare", 4, 12);

  spt.FuzzyInstructionMatchAt("Constant(#Field", 5, 9);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 5, 9);
  spt.FuzzyInstructionMatchAt("Constant(#5", 5, 14);
  spt.InstanceCallAt(5, 11, Token::kEQ);
  spt.FuzzyInstructionMatchAt("Branch if StrictCompare", 5, 11);

  spt.FuzzyInstructionMatchAt("LoadStaticField", 10, 10);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 10, 3);
  spt.FuzzyInstructionMatchAt("Return", 10, 3);
}


TEST_CASE(SourcePosition_LoadIndexed) {
  const char* kScript =
      "var x = 0;\n"
      "var z = new List(3);\n"
      "main() {\n"
      "  z[0];\n"
      "  var y = z[0] + z[1] + z[2];\n"
      "}\n";

  SourcePositionTest spt(thread, kScript);
  spt.BuildGraphFor("main");

  spt.FuzzyInstructionMatchAt("DebugStepCheck", 3, 5);
  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 3, 5);
  spt.StaticCallAt("get:z", 4, 3);
  spt.FuzzyInstructionMatchAt("Constant(#0)", 4, 5);
  spt.InstanceCallAt(4, 4, Token::kINDEX);

  spt.FuzzyInstructionMatchAt("Constant(#0)", 5, 13);
  spt.InstanceCallAt(5, 12, Token::kINDEX);
  spt.FuzzyInstructionMatchAt("Constant(#1)", 5, 20);
  spt.InstanceCallAt(5, 19, Token::kINDEX);

  spt.InstanceCallAt(5, 16, Token::kADD);

  spt.StaticCallAt("get:z", 5, 25);
  spt.FuzzyInstructionMatchAt("Constant(#2)", 5, 27);
  spt.InstanceCallAt(5, 26, Token::kINDEX);

  spt.InstanceCallAt(5, 23, Token::kADD);

  spt.FuzzyInstructionMatchAt("Constant(#null)", 6, 1);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 6, 1);
  spt.FuzzyInstructionMatchAt("Return", 6, 1);
}


TEST_CASE(SourcePosition_StoreIndexed) {
  const char* kScript =
      "var x = 0;\n"
      "var z = new List(4);\n"
      "main() {\n"
      "  z[0];\n"
      "  z[3] = z[0] + z[1] + z[2];\n"
      "}\n";

  SourcePositionTest spt(thread, kScript);
  spt.BuildGraphFor("main");

  spt.FuzzyInstructionMatchAt("DebugStepCheck", 3, 5);
  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 3, 5);
  spt.StaticCallAt("get:z", 4, 3);
  spt.FuzzyInstructionMatchAt("Constant(#0)", 4, 5);
  spt.InstanceCallAt(4, 4, Token::kINDEX);

  spt.FuzzyInstructionMatchAt("Constant(#3)", 5, 5);

  spt.StaticCallAt("get:z", 5, 10);
  spt.FuzzyInstructionMatchAt("Constant(#0)", 5, 12);
  spt.InstanceCallAt(5, 11, Token::kINDEX);

  spt.InstanceCallAt(5, 15, Token::kADD);

  spt.StaticCallAt("get:z", 5, 17);
  spt.FuzzyInstructionMatchAt("Constant(#1)", 5, 19);
  spt.InstanceCallAt(5, 18, Token::kINDEX);

  spt.StaticCallAt("get:z", 5, 24);
  spt.FuzzyInstructionMatchAt("Constant(#2)", 5, 26);
  spt.InstanceCallAt(5, 25, Token::kINDEX);

  spt.InstanceCallAt(5, 4, Token::kASSIGN_INDEX);

  spt.FuzzyInstructionMatchAt("Constant(#null)", 6, 1);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 6, 1);
  spt.FuzzyInstructionMatchAt("Return", 6, 1);
}


TEST_CASE(SourcePosition_BitwiseOperations) {
  const char* kScript =
      "var x = 0;\n"
      "var y = 1;\n"
      "main() {\n"
      "  var z;\n"
      "  z = x & y;\n"
      "  z = x | y;\n"
      "  z = x ^ y;\n"
      "  z = ~z;\n"
      "  return z;\n"
      "}\n";

  SourcePositionTest spt(thread, kScript);
  spt.BuildGraphFor("main");

  spt.FuzzyInstructionMatchAt("DebugStepCheck", 3, 5);
  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 3, 5);

  spt.FuzzyInstructionMatchAt("DebugStepCheck", 4, 7);
  spt.FuzzyInstructionMatchAt("Constant(#null", 4, 7);
  spt.FuzzyInstructionMatchAt("StoreLocal(z", 4, 7);

  spt.FuzzyInstructionMatchAt("LoadStaticField", 5, 7);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 5, 11);
  spt.InstanceCallAt(5, 9, Token::kBIT_AND);
  spt.FuzzyInstructionMatchAt("StoreLocal(z", 5, 3);

  spt.FuzzyInstructionMatchAt("LoadStaticField", 6, 7);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 6, 11);
  spt.InstanceCallAt(6, 9, Token::kBIT_OR);
  spt.FuzzyInstructionMatchAt("StoreLocal(z", 6, 3);

  spt.FuzzyInstructionMatchAt("LoadStaticField", 7, 7);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 7, 11);
  spt.InstanceCallAt(7, 9, Token::kBIT_XOR);
  spt.FuzzyInstructionMatchAt("StoreLocal(z", 7, 3);

  spt.FuzzyInstructionMatchAt("LoadLocal(z", 8, 8);
  spt.InstanceCallAt(8, 7, Token::kBIT_NOT);
  spt.FuzzyInstructionMatchAt("StoreLocal(z", 8, 3);

  spt.FuzzyInstructionMatchAt("LoadLocal(z", 9, 10);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 9, 3);
  spt.FuzzyInstructionMatchAt("Return", 9, 3);
}


TEST_CASE(SourcePosition_IfElse) {
  const char* kScript =
      "var x = 5;\n"
      "var y = 5;\n"
      "main() {\n"
      "  if (x != 0) {\n"
      "    return x;\n"
      "  } else {\n"
      "    return y;\n"
      "  }\n"
      "}\n";

  SourcePositionTest spt(thread, kScript);
  spt.BuildGraphFor("main");
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 3, 5);
  spt.FuzzyInstructionMatchAt("CheckStackOverflow", 3, 5);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 4, 7);
  spt.InstanceCallAt(4, 9, Token::kEQ);
  spt.FuzzyInstructionMatchAt("Branch if StrictCompare", 4, 9);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 5, 12);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 5, 5);
  spt.FuzzyInstructionMatchAt("Return", 5, 5);
  spt.FuzzyInstructionMatchAt("LoadStaticField", 7, 12);
  spt.FuzzyInstructionMatchAt("DebugStepCheck", 7, 5);
  spt.FuzzyInstructionMatchAt("Return", 7, 5);
}

}  // namespace dart
