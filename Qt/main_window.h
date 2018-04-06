#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QTreeView>
#include "ui_main_window.h"
#include "filesystem.h"

namespace Ui
{
    class Main_window;
}

enum struct Relocation_type : int
{
	relocate = 1000,
	restore
};

struct Relocation_queue_item_data
{
	Relocation_queue_item_data(Relocation_type type, string item_path)
	{
		this->type = type;
		this->item_path = item_path;
	}
	Relocation_type type;
	string item_path;
};

enum struct Relocation_queue_item_type : int
{
	operation_type = QTableWidgetItem::UserType,
	item_path_type
};

inline
const char* to_string(Relocation_type type)
{
	switch(type)
	{
		case Relocation_type::relocate:
			return "Relocate";
		case Relocation_type::restore:
			return "Restore";
	}
	return "INVALID RELOCATION_TYPE";
}

//typedef QList<Relocation_queue_item> Relocation_queue_model;


//struct Item_model : public QAbstractItemModel
//{
//	vector<Relocation_queue_item>* list;

//	Item_model()
//	{
//		this->list = new vector<Relocation_queue_item>();
//	}


//	// QAbstractItemModel interface
//public:
//	QModelIndex index(int row, int column, const QModelIndex& parent) const override
//	{
//		if(column == 0)
//			return createIndex(row, column, &(list->at(row).type));
//		else if(column == 1)
//			return createIndex(row, column, &(list->at(row).item_path));
//		else
//			return QModelIndex();
//	}

//	QModelIndex parent(const QModelIndex& child) const override
//	{
//		return QModelIndex();
//	}

//	int rowCount(const QModelIndex& parent) const override
//	{
//		return list->size();
//	}

//	int columnCount(const QModelIndex& parent) const override
//	{
//		return 2;
//	}

//	QVariant data(const QModelIndex& index, int role) const override
//	{
//		return QVariant();
//	}
//};

// index(),
// parent(),
// rowCount(),
// columnCount(),
// and data().

Relocation_type type;
string item_path;

struct Relocation_queue_item : public QTableWidgetItem
{
	Relocation_queue_item(Relocation_type type)
		: QTableWidgetItem((int)Relocation_queue_item_type::operation_type)
	{
		this->setData(Qt::DisplayRole, QVariant(to_string(type)));
	}

	Relocation_queue_item(const char* path)
		: QTableWidgetItem((int)Relocation_queue_item_type::item_path_type)
	{
		this->setData(Qt::DisplayRole, QVariant(path));
	}
};

class Main_window : public QMainWindow
{
    Q_OBJECT
public:

	Ui::Main_window* ui;
	QFileSystemModel* fs_model;

	void add_item(Relocation_type type, const char* path)
	{
		auto& queue = ui->relocation_queue;
		int cur_row = queue->currentRow();
		queue->insertRow(cur_row);
		queue->setItem(cur_row, 0, new Relocation_queue_item(type));
		queue->setItem(cur_row, 1, new Relocation_queue_item(path));
	}

	explicit Main_window(QWidget *parent = 0) : QMainWindow(parent), ui(new Ui::Main_window)
	{
		ui->setupUi(this);

		fs_model = new QFileSystemModel;
		fs_model->setRootPath("C:\\dev");
		fs_model->setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
		fs_model->setResolveSymlinks(false);
		ui->filesystem_view->setModel(fs_model);
		//list = new QList<Relocation_queue_item>();
		//list->append(Relocation_queue_item(Relocation_type::relocate, "C:\\dev\\test_data"));
		//ui->relocation_queue->setModel(list);

		//relocation_queue = new QTableWidget(0,2);
		//relocation_queue

		//ui->relocation_queue->setModel(relocation_queue);
		auto& queue = ui->relocation_queue;
		queue->setRowCount(0);
		queue->setColumnCount(2);
		//queue->setColumnHidden(0, true);
		queue->setHorizontalHeaderLabels(QStringList({"Stuff", "other_stuff"}));
		queue->setFixedWidth(queue->columnWidth(0) + queue->columnWidth(1));
		add_item(Relocation_type::relocate, "C:\\dev\\test_data\\");
		add_item(Relocation_type::relocate, "C:\\dev\\all_the_data\\");
		add_item(Relocation_type::restore, "D:\\bak\\test_data\\");
		add_item(Relocation_type::relocate, "C:\\other_dir\\unused_programs\\");
	}

	~Main_window()
	{
			delete ui;
			delete fs_model;
	}


};

#endif // MAIN_WINDOW_H


//#include "main_window.h"
//#include "filesystem.h"
////#include <Windows.h>

//Main_window::Main_window(QWidget *parent) : QMainWindow(parent), ui(new Ui::Main_window)
//{
//    ui->setupUi(this);

//    model = new QFileSystemModel;
//    model->setRootPath("C:\\dev");
//    model->setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
//    model->setResolveSymlinks(false);
//    ui->tree_view->setModel(model);

//    //CreateWindowExW()
//}

//Main_window::~Main_window()
//{
//    delete ui;
//    delete model;
//}
