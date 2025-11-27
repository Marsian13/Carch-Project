/**
 * @file pseudo_formats.cpp
 * @brief 
 * @author Vishank Singh, https://github.com/VishankSingh
 */

#include "assembler/parser.h"
#include "common/instructions.h"
#include "vm/registers.h"
#include "utils.h"
#include "config.h"

#include <string>

bool Parser::parse_pseudo() {
  if (currentToken().value=="la") {
    if (peekToken(1).line_number==currentToken().line_number
        && peekToken(1).type==TokenType::GP_REGISTER
        && peekToken(2).line_number==currentToken().line_number
        && peekToken(2).type==TokenType::COMMA
        && peekToken(3).line_number==currentToken().line_number
        && peekToken(3).type==TokenType::LABEL_REF
        && (peekToken(4).type==TokenType::EOF_ || peekToken(4).line_number!=currentToken().line_number)
        ) {
      std::string reg = reg_alias_to_name.at(peekToken(1).value);
      std::string label = peekToken(3).value;

      if (symbol_table_.find(label)!=symbol_table_.end() && symbol_table_[label].isData) {
        uint64_t address = symbol_table_[label].address; // relative to data section (e.g., 0,8,16,...)
        uint64_t data_section_start = vm_config::config.getDataSectionStart();
        uint64_t symbol_addr = data_section_start + address;
        uint64_t pc = instruction_index_ * 4;
        int64_t offset = static_cast<int64_t>(symbol_addr) - static_cast<int64_t>(pc);
        int32_t hi20 = (offset + 0x800) >> 12;
        int32_t lo12 = offset - (hi20 << 12);

        ICUnit auipc_instr;
        auipc_instr.setOpcode("auipc");
        auipc_instr.setRd(reg);
        auipc_instr.setRs1("");
        auipc_instr.setRs2("");
        auipc_instr.setImm(std::to_string(hi20));
        auipc_instr.setLineNumber(currentToken().line_number);

        // std::cout << "auipc " << reg << ", " << "0x" << std::hex << hi20 << std::dec << std::endl;

        ICUnit addi_instr;
        addi_instr.setOpcode("addi");
        addi_instr.setRd(reg);
        addi_instr.setRs1(reg);
        addi_instr.setRs2("");
        addi_instr.setImm(std::to_string(lo12));
        addi_instr.setLineNumber(currentToken().line_number);

        // std::cout << "addi " << reg << ", " << reg << ", " << lo12 << std::dec << std::endl;

        auipc_instr.setInstructionIndex(instruction_index_);
        intermediate_code_.emplace_back(auipc_instr, true);
        instruction_number_line_number_mapping_[instruction_index_] = auipc_instr.getLineNumber();
        instruction_index_++;

        addi_instr.setInstructionIndex(instruction_index_);
        intermediate_code_.emplace_back(addi_instr, true);
        instruction_number_line_number_mapping_[instruction_index_] = addi_instr.getLineNumber();
        instruction_index_++;
      } else {
        errors_.count++;
        recordError(ParseError(currentToken().line_number, "Invalid label reference"));
        errors_.all_errors.emplace_back(
          errors::InvalidLabelRefError(
            "Invalid label reference",
            "Expected: Label defined in .data section",
            filename_,
            currentToken().line_number,
            currentToken().column_number,
            GetLineFromFile(filename_, currentToken().line_number)));
      }
      skipCurrentLine();
      // instruction_index_+=2;
      return true;
    }
    return false;
  }

  // nop
  else if (currentToken().value=="nop") {
    if (peekToken(1).type==TokenType::EOF_
        || peekToken(1).line_number!=currentToken().line_number) {
      ICUnit block;
      block.setOpcode(currentToken().value);
      block.setLineNumber(currentToken().line_number);
      block.setInstructionIndex(instruction_index_);
      block.setOpcode("addi");
      block.setRd("x0");
      block.setRs1("x0");
      // block.setRs2("x0");
      block.setImm("0");
      intermediate_code_.emplace_back(block, true);
      instruction_number_line_number_mapping_[instruction_index_] = block.getLineNumber();
      instruction_index_++;
      nextToken();
      return true;
    }
    return false;
  }
  
  // li
  else if (currentToken().value=="li") {
    if (peekToken(1).line_number==currentToken().line_number
        && peekToken(1).type==TokenType::GP_REGISTER
        && peekToken(2).line_number==currentToken().line_number
        && peekToken(2).type==TokenType::COMMA
        && peekToken(3).line_number==currentToken().line_number
        && peekToken(3).type==TokenType::NUM
        &&
            (peekToken(4).type==TokenType::EOF_ || peekToken(4).line_number!=currentToken().line_number)) {
      ICUnit block;
      block.setOpcode(currentToken().value);
      int64_t imm = std::stoll(peekToken(3).value);
      std::string reg = reg_alias_to_name.at(peekToken(1).value);
      if (-2048 <= imm && imm <= 2047) {
        block.setLineNumber(currentToken().line_number);
        block.setInstructionIndex(instruction_index_);
        block.setOpcode("addi");
        block.setRd(reg);
        block.setRs1("x0");
        block.setImm(peekToken(3).value);
        intermediate_code_.emplace_back(block, true);
        instruction_number_line_number_mapping_[instruction_index_++] = block.getLineNumber();
      } else if (-2147483648LL <= imm && imm <= 2147483647LL) {
        int64_t upper = (imm + (1 << 11)) >> 12;
        int64_t lower = imm - (upper << 12);

        ICUnit luiBlock;
        luiBlock.setLineNumber(currentToken().line_number);
        luiBlock.setInstructionIndex(instruction_index_);
        luiBlock.setOpcode("lui");
        luiBlock.setRd(reg);
        luiBlock.setImm(std::to_string(upper));
        intermediate_code_.emplace_back(luiBlock, true);
        instruction_number_line_number_mapping_[instruction_index_++] = luiBlock.getLineNumber();

        if (lower!=0) {
          ICUnit addiBlock;
          addiBlock.setLineNumber(currentToken().line_number);
          addiBlock.setInstructionIndex(instruction_index_);
          addiBlock.setOpcode("addi");
          addiBlock.setRd(reg);
          addiBlock.setRs1(reg);
          addiBlock.setImm(std::to_string(lower));
          intermediate_code_.emplace_back(addiBlock, true);
          instruction_number_line_number_mapping_[instruction_index_++] = addiBlock.getLineNumber();
        }
      } 
      else if (INT64_MIN <= imm && imm <= INT64_MAX) {
        errors_.count++;
        recordError(ParseError(currentToken().line_number, "Immediate value out of range"));
        errors_.all_errors.emplace_back(
          errors::ImmediateOutOfRangeError(
            "Immediate value out of range",
            "Expected: -2^31 <= imm <= 2^31 - 1",
            filename_,
            currentToken().line_number,
            currentToken().column_number,
            GetLineFromFile(filename_, currentToken().line_number)
          )
        );
      }
      else {
        errors_.count++;
        recordError(ParseError(currentToken().line_number, "Immediate value out of range"));
        errors_.all_errors.emplace_back(
          errors::ImmediateOutOfRangeError(
            "Immediate value out of range",
            "Expected: -2^31 <= imm <= 2^31 - 1",
            filename_,
            currentToken().line_number,
            currentToken().column_number,
            GetLineFromFile(filename_, currentToken().line_number)
          )
        );
      }
      skipCurrentLine();
      return true;
    }
    return false;
  }
  
  // mv
  else if (currentToken().value=="mv") {
    if (peekToken(1).line_number==currentToken().line_number
        && peekToken(1).type==TokenType::GP_REGISTER
        && peekToken(2).line_number==currentToken().line_number
        && peekToken(2).type==TokenType::COMMA
        && peekToken(3).line_number==currentToken().line_number
        && peekToken(3).type==TokenType::GP_REGISTER
        &&
            (peekToken(4).type==TokenType::EOF_ || peekToken(4).line_number!=currentToken().line_number)) {
      ICUnit block;
      block.setOpcode("add");
      block.setLineNumber(currentToken().line_number);
      block.setInstructionIndex(instruction_index_);
      std::string reg;
      reg = reg_alias_to_name.at(peekToken(1).value);
      block.setRd(reg);
      reg = reg_alias_to_name.at(peekToken(3).value);
      block.setRs1(reg);
      block.setRs2("x0");
      intermediate_code_.emplace_back(block, true);
      instruction_number_line_number_mapping_[instruction_index_] = block.getLineNumber();
      instruction_index_++;
      skipCurrentLine();
      return true;
    }
    return false;
  }
  
  // not
  else if (currentToken().value=="not") {
    if (peekToken(1).line_number==currentToken().line_number
        && peekToken(1).type==TokenType::GP_REGISTER
        && peekToken(2).line_number==currentToken().line_number
        && peekToken(2).type==TokenType::COMMA
        && peekToken(3).line_number==currentToken().line_number
        && peekToken(3).type==TokenType::GP_REGISTER
        &&
            (peekToken(4).type==TokenType::EOF_ || peekToken(4).line_number!=currentToken().line_number)) {
      ICUnit block;
      block.setOpcode("xori");
      block.setLineNumber(currentToken().line_number);
      block.setInstructionIndex(instruction_index_);
      std::string reg = reg_alias_to_name.at(peekToken(1).value);
      block.setRd(reg);
      reg = reg_alias_to_name.at(peekToken(3).value);
      block.setRs1(reg);
      block.setImm("-1");
      intermediate_code_.emplace_back(block, true);
      instruction_number_line_number_mapping_[instruction_index_] = block.getLineNumber();
      instruction_index_++;
      skipCurrentLine();
      return true;
    }
    return false;
  }

  // ret
  else if (currentToken().value=="ret") {
    if (peekToken(1).type==TokenType::EOF_
        || peekToken(1).line_number!=currentToken().line_number) {
      ICUnit block;
      block.setOpcode("jalr");
      block.setLineNumber(currentToken().line_number);
      block.setInstructionIndex(instruction_index_);
      block.setRd("x0");
      block.setRs1("x1");
      block.setImm("0");
      intermediate_code_.emplace_back(block, true);
      instruction_number_line_number_mapping_[instruction_index_] = block.getLineNumber();
      instruction_index_++;
      nextToken();
      return true;
    }
    return false;
  }

  // invmod
  else if (currentToken().value == "invmod") {
    if (peekToken(1).line_number == currentToken().line_number
        && peekToken(1).type == TokenType::GP_REGISTER
        && peekToken(2).line_number == currentToken().line_number
        && peekToken(2).type == TokenType::COMMA
        && peekToken(3).line_number == currentToken().line_number
        && peekToken(3).type == TokenType::GP_REGISTER
        && peekToken(4).line_number == currentToken().line_number
        && peekToken(4).type == TokenType::COMMA
        && peekToken(5).line_number == currentToken().line_number
        && peekToken(5).type == TokenType::GP_REGISTER
        && (peekToken(6).type == TokenType::EOF_ || peekToken(6).line_number != currentToken().line_number)) {

      std::string rd  = reg_alias_to_name.at(peekToken(1).value);
      std::string rs1 = reg_alias_to_name.at(peekToken(3).value);
      std::string rs2 = reg_alias_to_name.at(peekToken(5).value);

      // our inst is invmod rd, rs1, rs2
      // invmod ans, a, m
      
      // step 1: gcd ans, a, m
      {
        ICUnit block;
        block.setOpcode("gcd");
        block.setLineNumber(currentToken().line_number);
        block.setInstructionIndex(instruction_index_);
        block.setRd(rd);
        block.setRs1(rs1);
        block.setRs2(rs2);
        intermediate_code_.emplace_back(block, true);
        instruction_number_line_number_mapping_[instruction_index_++] = block.getLineNumber();
      }
      
      // step 2: check ans, ans, x0 -> if gcd != 1 then ans = -1 else ans itself
      {
        ICUnit block;
        block.setOpcode("check");
        block.setLineNumber(currentToken().line_number);
        block.setInstructionIndex(instruction_index_);
        block.setRd(rd);
        block.setRs1(rd);
        block.setRs2("x0");
        intermediate_code_.emplace_back(block, true);
        instruction_number_line_number_mapping_[instruction_index_++] = block.getLineNumber();
      }

      // step 3: isprime ans, m, ans -> if ans is already -1 it will keep -1 also in the case if its not a prime
      {
        ICUnit block;
        block.setOpcode("isprime");
        block.setLineNumber(currentToken().line_number);
        block.setInstructionIndex(instruction_index_);
        block.setRd(rd);
        block.setRs1(rs2);
        block.setRs2(rd);
        intermediate_code_.emplace_back(block, true);
        instruction_number_line_number_mapping_[instruction_index_++] = block.getLineNumber();
      }

      // step 4: setmod x0, m, ans -> if ans is -1 then will set global variable as -1
      {
        ICUnit block;
        block.setOpcode("setmod");
        block.setLineNumber(currentToken().line_number);
        block.setInstructionIndex(instruction_index_);
        block.setRd("x0");
        block.setRs1(rs2);
        block.setRs2(rd);
        intermediate_code_.emplace_back(block, true);
        instruction_number_line_number_mapping_[instruction_index_++] = block.getLineNumber();
      }

      // step 5: addi m, m, -2 -> as we are using Fermat’s little theorem
      {
        ICUnit Block;
        Block.setOpcode("addi");
        Block.setLineNumber(currentToken().line_number);
        Block.setInstructionIndex(instruction_index_);
        Block.setRd(rs2);
        Block.setRs1(rs2);
        Block.setImm("-2");   // rs2 = rs2 - 2
        intermediate_code_.emplace_back(Block, true);
        instruction_number_line_number_mapping_[instruction_index_++] = Block.getLineNumber();
      }

      // step 6: binexp ans, a, m-2 -> here if ans is -1 that means invmod do not exist
      {
        ICUnit block;
        block.setOpcode("powmod");
        block.setLineNumber(currentToken().line_number);
        block.setInstructionIndex(instruction_index_);
        block.setRd(rd);
        block.setRs1(rs1);
        block.setRs2(rs2);
        intermediate_code_.emplace_back(block, true);
        instruction_number_line_number_mapping_[instruction_index_++] = block.getLineNumber();
      }

      // step 7: addi m, m, 2 -> restoring the value of m
      {
        ICUnit Block;
        Block.setOpcode("addi");
        Block.setLineNumber(currentToken().line_number);
        Block.setInstructionIndex(instruction_index_);
        Block.setRd(rs2);
        Block.setRs1(rs2);
        Block.setImm("2");   // rs2 = rs2 + 2
        intermediate_code_.emplace_back(Block, true);
        instruction_number_line_number_mapping_[instruction_index_++] = Block.getLineNumber();
      }

      skipCurrentLine();
      return true;
    }
    return false;
  }


  return false;
}

