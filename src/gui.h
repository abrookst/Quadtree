#ifndef GUI_H
#define GUI_H

#include <string>
#include <sstream>
#include <queue>
#include <iostream>

// Console buffer for capturing cout
class ConsoleBuffer : public std::streambuf
{
public:
    static std::string& get_output();
    static void clear_output();
    static std::string get_command();
    static void submit_command(const std::string& cmd);

protected:
    virtual int overflow(int c) override;

private:
    static std::queue<std::string> s_command_queue;
};

// GUI rendering function
void gui_render();

#endif // GUI_H
