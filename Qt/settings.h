#pragma once
#include "fae_lib.h"

struct App_Settings
{
    string current_dir;
    string backup_dir;
    string test_data_dir;
    string test_data_source;
	bool confirm_on_quit;
};

extern App_Settings Settings;

inline
void init_settings()
{
    Settings.current_dir        = u8"C:\\dev\\";
    Settings.backup_dir         = u8"D:\\bak\\";
    Settings.test_data_dir      = u8"C:\\dev\\test_data\\";
    Settings.test_data_source   = u8"C:\\dev\\test_data_source\\";
    Settings.confirm_on_quit    = false;
}
