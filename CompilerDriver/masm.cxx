/*
 *	========================================================
 *
 *	C++Kit
 * 	Copyright Amlal El Mahrouss, all rights reserved.
 *
 * 	========================================================
 */

/////////////////////////////////////////////////////////////////////////////////////////

// @file masm.cxx
// @brief The MP-UX Assembler, outputs an AE object.
// This assembler was made for NewCPU, a brand-new RISC architecture.

// REMINDER: when dealing with an undefined symbol use (string size):ld:(string)
// so that ld will look for it.

/////////////////////////////////////////////////////////////////////////////////////////

#include <C++Kit/AsmKit/Arch/NewCPU.hpp>
#include <C++Kit/ParserKit.hpp>
#include <C++Kit/StdKit/PEF.hpp>
#include <C++Kit/StdKit/AE.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>

/////////////////////

// ANSI ESCAPE CODES

/////////////////////

#define kBlank  "\e[0;30m"
#define kRed    "\e[0;31m"
#define kWhite  "\e[0;97m"
#define kYellow "\e[0;33m"

static char           kOutputArch = CxxKit::kPefArchRISCV;

static std::vector<bool> kLabelLevel;

//! base relocation address for every mp-ux app.
static UInt32       kErrorLimit = 10;
static UInt32       kAcceptableErrors = 0;

static std::size_t  kCounter = 1UL;

static std::vector<char> kBytes;
static CxxKit::AERecordHeader kCurrentRecord{ .fName = "", .fKind = CxxKit::kPefCode, .fSize = 0, .fOffset = 0 };

static std::vector<CxxKit::AERecordHeader> kRecords;
static std::vector<std::string> kUndefinedSymbols;
static const std::string kUndefinedSymbol = ":ld:";

// \brief forward decl.
static std::string masm_check_line(std::string& line, const std::string& file);
static void masm_check_export(std::string& line);
static void masm_read_labels(std::string& line);
static void masm_read_instr(std::string& line, const std::string& file);

namespace detail
{
    void print_error(std::string reason, const std::string& file) noexcept
    {
        if (reason[0] == '\n')
            reason.erase(0, 1);

        std::cout << kRed << "[ masm ] " << kWhite << ((file == "masm") ? "internal assembler error " : ("in file, " + file)) << kBlank << std::endl;
        std::cout << kRed << "[ masm ] " << kWhite << reason << kBlank << std::endl;

        if (kAcceptableErrors > kErrorLimit)
            std::exit(3);

        ++kAcceptableErrors;
    }

    void print_warning(std::string reason, const std::string& file) noexcept
    {
        if (reason[0] == '\n')
            reason.erase(0, 1);

        if (!file.empty())
        {
            std::cout << kYellow << "[ file ] " << kWhite << file << kBlank << std::endl;
        }

        std::cout << kYellow << "[ masm ] " << kWhite << reason << kBlank << std::endl;
    }
}

// provide operator<< for AE

std::ofstream& operator<<(std::ofstream& fp, CxxKit::AEHeader& container)
{
    fp.write((char*)&container, sizeof(CxxKit::AEHeader));

    return fp;
}

std::ofstream& operator<<(std::ofstream& fp, CxxKit::AERecordHeader& container)
{
    fp.write((char*)&container, sizeof(CxxKit::AERecordHeader));

    return fp;
}

/////////////////////////////////////////////////////////////////////////////////////////

// @brief Main entrypoint.

/////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    for (size_t i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            if (strcmp(argv[i], "-v") == 0)
            {
                std::cout << "masm: The MP-UX Assembler.\nmasm: Copyright (c) 2023 Amlal El Mahrouss.\n";
                return 0;
            }

            if (strcmp(argv[i], "-marc") == 0)
            {
                kOutputArch = CxxKit::kPefArchARC;
                continue;
            }

            std::cout << "masm: ignore " << argv[i] << "\n";
            continue;
        }

        if (!std::filesystem::exists(argv[i]))
            continue;

        std::string object_output(argv[i]);

        if (object_output.find(kAsmFileExt) != std::string::npos)
        {
            object_output.erase(object_output.find(kAsmFileExt), std::size(kAsmFileExt));
        }

        object_output += kObjectFileExt;

        std::ifstream file_ptr(argv[i]);
        std::ofstream file_ptr_out(object_output,
                                   std::ofstream::binary);

        std::string line;

        CxxKit::AEHeader hdr{ 0 };

        memset(hdr.fPad, kAEInvalidOpcode, kAEPad);

        hdr.fMagic[0] = kAEMag0;
        hdr.fMagic[1] = kAEMag1;
        hdr.fSize = sizeof(CxxKit::AEHeader);
        hdr.fArch = kOutputArch;

        /////////////////////////////////////////////////////////////////////////////////////////

        // COMPILATION LOOP

        /////////////////////////////////////////////////////////////////////////////////////////

        while (std::getline(file_ptr, line))
        {
            if (auto ln = masm_check_line(line, argv[i]);
                !ln.empty())
            {
                detail::print_error(ln, argv[i]);
                continue;
            }

            if (ParserKit::find_word(line, "#"))
                continue;

            masm_check_export(line);
            masm_read_labels(line);
            masm_read_instr(line, argv[i]);
        }

        // this is the final step, write everything to the file.

        auto pos = file_ptr_out.tellp();

        hdr.fCount = kRecords.size() + kUndefinedSymbols.size();

        file_ptr_out << hdr;

        if (kRecords.empty())
        {
            std::filesystem::remove(object_output);
            return -1;
        }

        kRecords[kRecords.size() - 1].fSize = kBytes.size();

        std::size_t record_count = 0UL;

        for (auto& rec : kRecords)
        {
            rec.fFlags |= CxxKit::kKindRelocationAtRuntime;
            rec.fOffset = record_count;
            ++record_count;

            file_ptr_out << rec;
        }

        // increment once again, so that we won't lie about the kUndefinedSymbols.
        ++record_count;

        for (auto& sym : kUndefinedSymbols)
        {
            CxxKit::AERecordHeader _record_hdr{ 0 };

            _record_hdr.fKind = kAEInvalidOpcode;
            _record_hdr.fSize = sym.size();
            _record_hdr.fOffset = record_count;

            ++record_count;

            memset(_record_hdr.fPad, kAEInvalidOpcode, kAEPad);

            memcpy(_record_hdr.fName, sym.c_str(), sym.size());

            file_ptr_out << _record_hdr;

            ++kCounter;
        }

        auto pos_end = file_ptr_out.tellp();

        file_ptr_out.seekp(pos);

        hdr.fStartCode = pos_end;
        hdr.fCodeSize = kBytes.size();

        file_ptr_out << hdr;

        file_ptr_out.seekp(pos_end);

        // byte from byte, we write this.
        for (auto& byte : kBytes)
        {
            file_ptr_out.write(reinterpret_cast<const char *>(&byte), sizeof(byte));
        }

        file_ptr_out.flush();
        file_ptr_out.close();

        return 0;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

// @brief Check for exported symbols

/////////////////////////////////////////////////////////////////////////////////////////

static void masm_check_export(std::string& line)
{
    // __import is the opposite of export, it signals to the ld
    // that we need this symbol.
    if (ParserKit::find_word(line, "__import"))
    {
        auto name = line.substr(line.find("__import") + strlen("__import"));

        std::string result = std::to_string(name.size());
        result += kUndefinedSymbol;

        // mangle this
        for (char & j : name)
        {
            if (j == ' ' ||
                j == ',')
                j = '$';

        }

        result += name;

        if (name.find(".text") != std::string::npos)
        {
            // data is treated as code.
            kCurrentRecord.fKind = CxxKit::kPefCode;
        }
        else if (name.find(".data") != std::string::npos)
        {
            // no code will be executed from here.
            kCurrentRecord.fKind = CxxKit::kPefData;
        }
        else if (name.find(".page_zero") != std::string::npos)
        {
            // this is a bss section.
            kCurrentRecord.fKind = CxxKit::kPefZero;
        }

        // this is a special case for the start stub.
        // we want this so that ld can find it.

        if (name == "__start")
        {
            kCurrentRecord.fKind = CxxKit::kPefCode;
        }

        // now we can tell the code size of the previous kCurrentRecord.

        if (!kRecords.empty())
            kRecords[kRecords.size() - 1].fSize = kBytes.size();

        memset(kCurrentRecord.fName, 0, kAESymbolLen);
        memcpy(kCurrentRecord.fName, result.c_str(), result.size());

        ++kCounter;

        memset(kCurrentRecord.fPad, kAEInvalidOpcode, kAEPad);

        kRecords.emplace_back(kCurrentRecord);

        return;
    }

    // __export is a special keyword used by masm to tell the AE output stage to mark this section as a header.
    // it currently supports .text, .data., page_zero
    if (ParserKit::find_word(line, "__export"))
    {
        auto name = line.substr(line.find("__export") + strlen("__export"));

        for (char& j : name)
        {
            if (j == ' ')
                j = '$';

        }

        if (name.find(',') != std::string::npos)
            name.erase(name.find(','));

        if (name.find(".text") != std::string::npos)
        {
            // data is treated as code.
            kCurrentRecord.fKind = CxxKit::kPefCode;
        }
        else if (name.find(".data") != std::string::npos)
        {
            // no code will be executed from here.
            kCurrentRecord.fKind = CxxKit::kPefData;
        }
        else if (name.find(".page_zero") != std::string::npos)
        {
            // this is a bss section.
            kCurrentRecord.fKind = CxxKit::kPefZero;
        }

        // this is a special case for the start stub.
        // we want this so that ld can find it.

        if (name == "__start")
        {
            kCurrentRecord.fKind = CxxKit::kPefCode;
        }

        // now we can tell the code size of the previous kCurrentRecord.

        if (!kRecords.empty())
            kRecords[kRecords.size() - 1].fSize = kBytes.size();

        memset(kCurrentRecord.fName, 0, kAESymbolLen);
        memcpy(kCurrentRecord.fName, name.c_str(), name.size());

        ++kCounter;

        memset(kCurrentRecord.fPad, kAEInvalidOpcode, kAEPad);

        kRecords.emplace_back(kCurrentRecord);
    }
}

// \brief algorithms and helpers.

namespace detail::algorithm
{
    // \brief authorize a brief set of characters.
    static inline bool is_not_alnum_space(char c)
    {
        return !(isalpha(c) || isdigit(c) || (c == ' ') || (c == '\t') || (c == ',') ||
                (c == '(') || (c == ')') || (c == '"') || (c == '\'') || (c == '[') || (c == ']')
                || (c == '+') || (c == '_'));
    }

    bool is_valid(const std::string &str)
    {
        if (ParserKit::find_word(str, "__export") ||
                ParserKit::find_word(str, "__import"))
            return true;

        return find_if(str.begin(), str.end(), is_not_alnum_space) == str.end();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

// @brief Check for line (syntax check)

/////////////////////////////////////////////////////////////////////////////////////////

static std::string masm_check_line(std::string& line, const std::string& file)
{
    (void)file;

    std::string err_str;

    while (line.find('\t') != std::string::npos)
        line.erase(line.find('\t'), 1);

    if (line.empty() ||
        ParserKit::find_word(line, "__import") ||
        ParserKit::find_word(line, "__export") ||
        ParserKit::find_word(line, "begin")  ||
        ParserKit::find_word(line, "end") ||
        ParserKit::find_word(line, "#") ||
        ParserKit::find_word(line, "layout"))
    {
        if (line.find('#') != std::string::npos)
        {
            line.erase(line.find('#'));
        }

        return err_str;
    }

    if (!detail::algorithm::is_valid(line))
    {
        err_str = "Line contains non alphanumeric characters.\nhere -> ";
        err_str += line;

        return err_str;
    }

    // check for a valid instruction format.

    if (line.find(',') != std::string::npos)
    {
        if (line.find(',') + 1 == line.size())
        {
            err_str += "\ninstruction lacks right register, here -> ";
            err_str += line.substr(line.find(','));

            return err_str;
        }
        else
        {
            bool nothing_on_right = true;

            if (line.find(',') + 1 > line.size())
            {
                err_str += "\ninstruction not complete, here -> ";
                err_str += line;

                return err_str;
            }

            auto substr = line.substr(line.find(',') + 1);

            for (auto& ch : substr)
            {
                if (ch != ' ' &&
                    ch != '\t')
                {
                    nothing_on_right = false;
                }
            }

            // this means we found nothing after that ',' .
            if (nothing_on_right)
            {
                err_str += "\ninstruction not complete, here -> ";
                err_str += line;

                return err_str;
            }
        }
    }

    std::vector<std::string> opcodes_list = { "jb", "psh", "stw", "ldw", "lda" };

    for (auto& opcodes : kOpcodesStd)
    {
        if (line.find(opcodes.fName) != std::string::npos)
        {
            for (auto& op : opcodes_list)
            {
                if (line == op ||
                    line.find(op) != std::string::npos &&
                    !isspace(line[line.find(op) + op.size()]))
                {
                    err_str += "\nmalformed ";
                    err_str += op;
                    err_str += " instruction, here -> ";
                    err_str += line;
                }
            }

            return err_str;
        }
    }

    err_str += "Unknown syntax, ";
    err_str += line;

    return err_str;
}

/////////////////////////////////////////////////////////////////////////////////////////

// @brief Check if a line starts a new label.

/////////////////////////////////////////////////////////////////////////////////////////

static void masm_read_labels(std::string& line)
{
    if (ParserKit::find_word(line, "begin"))
    {
        kLabelLevel.emplace_back(true);
    }
    else if (ParserKit::find_word(line, "end"))
    {
        kLabelLevel.pop_back();
    }
}

namespace detail
{
    union number_type
    {
        explicit number_type(UInt64 raw)
                : raw(raw)
        {}

        char number[8];
        UInt64 raw;
    };
}

static bool masm_write_number(std::size_t pos, std::string& jump_label)
{
    switch (jump_label[pos+1])
    {
        case 'x':
        {
            if (auto res = strtoq(jump_label.substr(pos + 2).c_str(),
                                  nullptr, 2);
                    !res)
            {
                if (errno != 0)
                {
                    return false;
                }
            }

            detail::number_type num(strtoq(jump_label.substr(pos + 2).c_str(),
                                   nullptr, 16));

            for (char i : num.number)
            {
                kBytes.push_back(i);
            }

            return true;
        }
        case 'b':
        {
            if (auto res = strtoq(jump_label.substr(pos + 2).c_str(),
                                  nullptr, 2);
                !res)
            {
                if (errno != 0)
                {
                    return false;
                }
            }

            detail::number_type num(strtoq(jump_label.substr(pos + 2).c_str(),
                                   nullptr, 2));

            for (char i : num.number)
            {
                kBytes.push_back(i);
            }

            return true;
        }
        default:
        {
            break;
        }
    }

    /* check for errno and stuff like that */
    if (auto res = strtoq(jump_label.substr(pos).c_str(),
                          nullptr, 10);
            !res)
    {
        if (errno != 0)
        {
            return false;
        }
    }
    else
    {
        detail::number_type num(strtoq(jump_label.substr(pos).c_str(),
                                       nullptr, 10));

        for (char i : num.number)
        {
            kBytes.push_back(i);
        }

        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////

// @brief Read and write instruction to file.

/////////////////////////////////////////////////////////////////////////////////////////

static void masm_read_instr(std::string& line, const std::string& file)
{
    for (auto& opcodes : kOpcodesStd)
    {
        if (ParserKit::find_word(line, opcodes.fName))
        {
            std::string name(opcodes.fName);
            std::string jump_label, cpy_jump_label;

            kBytes.emplace_back(opcodes.fOpcode);
            kBytes.emplace_back(opcodes.fFunct3);
            kBytes.emplace_back(opcodes.fFunct7);

            // check funct7
            switch (opcodes.fFunct7)
            {
                // reg to reg means register to register transfer operation.
                case kAsmRegToReg:
                case kAsmImmediate:
                {
                    // \brief how many registers we found.
                    std::size_t found_some = 0UL;

                    for (int reg_index = 0; reg_index < kAsmRegisterLimit; ++reg_index)
                    {
                        std::string register_syntax = kAsmRegisterPrefix;
                        register_syntax += std::to_string(reg_index);

                        // if we found one
                        if (ParserKit::find_word(line, register_syntax))
                        {
                            // emplace it.
                            kBytes.emplace_back(reg_index);
                            ++found_some;
                        }
                    }

                    if (opcodes.fFunct7 != kAsmImmediate)
                    {
                        // remember! register to register!
                        if (found_some == 1)
                        {
                            detail::print_error("unrecognized register found.\ntip: each NewCPU register starts with 'r'.\nline: " + line, file);
                        }
                    }

                    if (found_some < 1 &&
                        name != "psh")
                    {
                        detail::print_error("invalid combination of opcode and registers.\nline: " + line, file);
                    }

                    if (found_some > 0 &&
                        name == "pop")
                    {
                        detail::print_error("invalid combination of opcode and register for 'pop'.\nline: " + line, file);
                    }
                }
                default:
                    break;

            }

            // try to fetch a number from the name
            if (name == "psh" ||
                name == "jb" ||
                name == "stw" ||
                name == "ldw" ||
                name == "lda")
            {
                auto where_string = name;

                if (name == "stw" ||
                    name == "ldw" ||
                    name == "lda")
                    where_string = ",";

                jump_label = line.substr(line.find(where_string) + where_string.size());
                cpy_jump_label = jump_label;

                // replace any spaces with $
                if (jump_label[0] == ' ')
                {
                    while (jump_label.find(' ') != std::string::npos)
                    {
                        if (isalnum(jump_label[0]) ||
                            isdigit(jump_label[0]))
                            break;

                        jump_label.erase(jump_label.find(' '), 1);
                    }
                }

                if (!masm_write_number(0, jump_label))
                {
                    goto masm_write_label;
                }
            }

            // if jump to branch
            if (name == "jb")
            {
masm_write_label:
                if (cpy_jump_label.find('\n') != std::string::npos)
                    cpy_jump_label.erase(cpy_jump_label.find('\n'), 1);

                if (cpy_jump_label.find("__import") != std::string::npos)
                    cpy_jump_label.erase(cpy_jump_label.find("__import"), strlen("__import"));

                while (cpy_jump_label.find(' ') != std::string::npos)
                    cpy_jump_label.erase(cpy_jump_label.find(' '), 1);

                while (cpy_jump_label.find(',') != std::string::npos)
                {
                    detail::print_error("internal assembler error", "masm");
                }

                auto mld_reloc_str = std::to_string(cpy_jump_label.size());
                mld_reloc_str += ":mld_reloc:";
                mld_reloc_str += cpy_jump_label;

                bool ignore_back_slash = false;

                for (auto& reloc_chr : mld_reloc_str)
                {
                    if (reloc_chr == '\\')
                    {
                        ignore_back_slash = true;
                        continue;
                    }

                    if (ignore_back_slash)
                    {
                        ignore_back_slash = false;
                        continue;
                    }

                    kBytes.push_back(reloc_chr);
                }
            }

            kBytes.push_back(0);
        }
    }

}