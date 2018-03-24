#pragma once
#include "fae_lib.h"

struct App_Settings
{
	wxFileName current_dir;
	wxFileName backup_dir;
	wxFileName test_data_dir;
	wxFileName test_data_source;
	bool confirm_on_quit;
};

extern App_Settings Settings;

void init_settings()
{
	Settings.current_dir = wxFileName(u8"C:\\dev\\");
	Settings.backup_dir = wxFileName(u8"D:\\bak\\");
	Settings.test_data_dir = wxFileName(u8"C:\\dev\\test_data\\");
	Settings.test_data_source = wxFileName(u8"C:\\dev\\test_data_source\\");
	Settings.confirm_on_quit = false;
}