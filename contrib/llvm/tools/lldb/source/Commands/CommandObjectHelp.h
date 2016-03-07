//===-- CommandObjectHelp.h -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_CommandObjectHelp_h_
#define liblldb_CommandObjectHelp_h_

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/CommandObject.h"
#include "lldb/Interpreter/Options.h"

namespace lldb_private {

//-------------------------------------------------------------------------
// CommandObjectHelp
//-------------------------------------------------------------------------

class CommandObjectHelp : public CommandObjectParsed
{
public:

    CommandObjectHelp (CommandInterpreter &interpreter);

    ~CommandObjectHelp() override;

    int
    HandleCompletion(Args &input,
		     int &cursor_index,
		     int &cursor_char_position,
		     int match_start_point,
		     int max_return_elements,
		     bool &word_complete,
		     StringList &matches) override;
    
    class CommandOptions : public Options
    {
    public:
        
        CommandOptions (CommandInterpreter &interpreter) :
        Options (interpreter)
        {
        }
        
        ~CommandOptions() override {}
        
        Error
        SetOptionValue(uint32_t option_idx, const char *option_arg) override
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;
            
            switch (short_option)
            {
                case 'a':
                    m_show_aliases = false;
                    break;
                case 'u':
                    m_show_user_defined = false;
                    break;
                case 'h':
                    m_show_hidden = true;
                    break;
                default:
                    error.SetErrorStringWithFormat ("unrecognized option '%c'", short_option);
                    break;
            }
            
            return error;
        }
        
        void
        OptionParsingStarting() override
        {
            m_show_aliases = true;
            m_show_user_defined = true;
            m_show_hidden = false;
        }
        
        const OptionDefinition*
        GetDefinitions() override
        {
            return g_option_table;
        }
        
        // Options table: Required for subclasses of Options.
        
        static OptionDefinition g_option_table[];
        
        // Instance variables to hold the values for command options.
        
        bool m_show_aliases;
        bool m_show_user_defined;
        bool m_show_hidden;
    };
    
    Options *
    GetOptions() override
    {
        return &m_options;
    }
    
protected:
    bool
    DoExecute(Args& command,
	      CommandReturnObject &result) override;
    
private:
    CommandOptions m_options;
};

} // namespace lldb_private

#endif // liblldb_CommandObjectHelp_h_
