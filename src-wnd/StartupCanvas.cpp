/* DarkMark (C) 2019 Stephane Charette <stephanecharette@gmail.com>
 * $Id$
 */

#include "DarkMark.hpp"

#include "json.hpp"
using json = nlohmann::json;


dm::StartupCanvas::StartupCanvas(const std::string & key, const std::string & dir) :
	Component("Startup Notebook Canvas"),
	cfg_key(key),
	hide_some_weight_files("hide extra .weights files"),
	applying_filter(true),
	done(false)
{
	addAndMakeVisible(pp);
	addAndMakeVisible(table);
	addAndMakeVisible(thumbnail);
	addAndMakeVisible(hide_some_weight_files);

	table.getHeader().addColumn("filename"	, 1, 100, 30, -1, TableHeaderComponent::notSortable);
	table.getHeader().addColumn("type"		, 2, 100, 30, -1, TableHeaderComponent::notSortable);
	table.getHeader().addColumn("size"		, 3, 100, 30, -1, TableHeaderComponent::notSortable);
	table.getHeader().addColumn("timestamp"	, 4, 100, 30, -1, TableHeaderComponent::notSortable);
	// if changing columns, also update paintCell() below

	table.getHeader().setStretchToFitActive(true);
	table.getHeader().setPopupMenuActive(false);
	table.setModel(this);

	project_directory	= dir.c_str();
	number_of_images	= "...";
	number_of_json		= "...";
	number_of_classes	= "...";
	number_of_marks		= "...";
	newest_markup		= "...";
	oldest_markup		= "...";

	darknet_configuration_filename	= cfg().getValue("project_" + key + "_cfg"		);
	darknet_weights_filename		= cfg().getValue("project_" + key + "_weights"	);
	darknet_names_filename			= cfg().getValue("project_" + key + "_names"	);

	darknet_configuration_filename	.addListener(this);
	darknet_weights_filename		.addListener(this);
	darknet_names_filename			.addListener(this);

	Array<PropertyComponent *> properties;

	properties.add(new TextPropertyComponent(project_directory				, "project directory"		, 1000, false, false));
	properties.add(new TextPropertyComponent(number_of_images				, "image files"				, 1000, false, false));
	properties.add(new TextPropertyComponent(number_of_json					, "markup files"			, 1000, false, false));
	properties.add(new TextPropertyComponent(number_of_classes				, "number of classes"		, 1000, false, false));
	properties.add(new TextPropertyComponent(number_of_marks				, "number of marks"			, 1000, false, false));
	properties.add(new TextPropertyComponent(newest_markup					, "newest markup"			, 1000, false, false));
	properties.add(new TextPropertyComponent(oldest_markup					, "oldest markup"			, 1000, false, false));
	properties.add(new TextPropertyComponent(darknet_configuration_filename	, "darknet configuration"	, 1000, false, true));
	properties.add(new TextPropertyComponent(darknet_weights_filename		, "darknet weights"			, 1000, false, true));
	properties.add(new TextPropertyComponent(darknet_names_filename			, "classes/names"			, 1000, false, true));

	pp.addProperties(properties);
	properties.clear();

	hide_some_weight_files.setToggleState(true, NotificationType::sendNotification);
	hide_some_weight_files.addListener(this);

	t = std::thread(&StartupCanvas::initialize_on_thread, this);

	return;
}


dm::StartupCanvas::~StartupCanvas()
{
	done = true;
	t.join();

	return;
}


void dm::StartupCanvas::resized()
{
	const int margin_size		= 5;
	const int number_of_lines	= 10;
	const int height_per_line	= 25;
	const int total_pp_height	= number_of_lines * height_per_line;

	FlexBox bottom;
	bottom.flexDirection	= FlexBox::Direction::row;
	bottom.alignContent		= FlexBox::AlignContent::stretch;
	bottom.justifyContent	= FlexBox::JustifyContent::spaceBetween;
	bottom.items.add(FlexItem(thumbnail				).withFlex(3.0));
	bottom.items.add(FlexItem(hide_some_weight_files).withFlex(1.0));

	FlexBox fb;
	fb.flexDirection = FlexBox::Direction::column;
	fb.items.add(FlexItem(pp		).withHeight(total_pp_height));
	fb.items.add(FlexItem(/*spacer*/).withHeight(margin_size));
	fb.items.add(FlexItem(table		).withFlex(3.0));
	fb.items.add(FlexItem(/*spacer*/).withHeight(margin_size));
	fb.items.add(FlexItem(bottom	).withFlex(1.0));

	auto r = getLocalBounds();
	r.reduce(margin_size, margin_size);
	fb.performLayout(r);

	return;
}


void dm::StartupCanvas::initialize_on_thread()
{
	applying_filter = true;
	v.clear();
	table.updateContent();
	table.repaint();

	// give the main window a chance to start up completely before we start pounding the drive looking for files
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	File dir(project_directory.toString());

	VStr image_filenames;
	VStr json_filenames;
	find_files(dir, image_filenames, json_filenames, done);
	if (not done)
	{
		const size_t image_counter	= image_filenames.size();
		const size_t json_counter	= json_filenames.size();

		number_of_images	= String(image_counter);
		number_of_json		= String(json_counter) + " (" + String(std::round(100.0 * json_counter / (image_counter == 0 ? 1 : image_counter))) + "%)";
	}

	if (image_filenames.empty() == false)
	{
		auto image = ImageCache::getFromFile(File(image_filenames[std::rand() % image_filenames.size()]));
		thumbnail.setImage(image, RectanglePlacement::xLeft);
	}

	find_all_darknet_files();

	try
	{
		// look for newest and oldest timestamp
		std::time_t oldest = 0;
		std::time_t newest = 0;
		std::size_t count = 0;

		for (const auto & filename : json_filenames)
		{
			if (done)
			{
				break;
			}

			json j = json::parse(File(filename).loadFileAsString().toStdString());
			count += j["mark"].size();
			std::time_t timestamp = j["timestamp"].get<std::time_t>();
			if (oldest == 0 || timestamp < oldest)
			{
				oldest = timestamp;
			}
			if (newest == 0 || timestamp > newest)
			{
				newest = timestamp;
			}
		}

		char buffer[50] = "";
		if (oldest > 0)
		{
			auto tm = localtime(&oldest);
			std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
		}
		oldest_markup = buffer;

		buffer[0] = '\0';
		if (newest > 0)
		{
			auto tm = localtime(&newest);
			std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
		}
		newest_markup = buffer;

		const int classes = number_of_classes.getValue();
		std::stringstream ss;
		ss << count;
		if (classes > 0)
		{
			ss << " (average of " << (count/classes) << " marks per class)";
		}
		number_of_marks = ss.str().c_str();
	}
	catch (const std::exception & e)
	{
		Log(dir.getFullPathName().toStdString() + ": error while reading JSON: " + e.what());
		oldest_markup	= "(error reading markup .json file)";
		newest_markup	= oldest_markup.toString();
		number_of_marks	= oldest_markup.toString();
	}

	return;
}


void dm::StartupCanvas::find_all_darknet_files()
{
	// find all the .cfg, .weight, and .names files

	applying_filter = true;
	v.clear();
	table.updateContent();
	table.repaint();

	extra_weights_files.clear();

	for (const auto type : {DarknetFileInfo::EType::kCfg, DarknetFileInfo::EType::kWeight, DarknetFileInfo::EType::kNames})
	{
		if (done)
		{
			break;
		}

		const String extension =
				type == DarknetFileInfo::EType::kCfg	?	"*.cfg"		:
				type == DarknetFileInfo::EType::kWeight	?	"*.weights" :
															"*.name*"	;

		File dir(project_directory.toString());
		DirectoryIterator iter(dir, false, extension, File::findFiles + File::ignoreHiddenFiles);
		while (iter.next() && done == false)
		{
			File f = iter.getFile();

			DarknetFileInfo dfi;
			dfi.type		= type;
			dfi.full_name	= f.getFullPathName().toStdString();
			dfi.short_name	= f.getFileName().toStdString();
			dfi.file_size	= File::descriptionOfSizeInBytes(f.getSize()).toStdString();
			dfi.timestamp	= f.getLastModificationTime().formatted("%Y-%m-%d %H:%M:%S").toStdString();
			v.push_back(dfi);
		}
	}

	/* Sort by type, then within each type sort by timestamp so the newest files appear at the top.  If the type and
	 * timestamps are the same (e.g., maybe the files were rsync'd without times being maintained) then compare the
	 * names of the files themselves alphabetically so that yolov3-tiny_39000.weights appears prior to
	 * yolov3-tiny_38000.weights.
	 */
	std::sort(v.begin(), v.end(),
			[](const DarknetFileInfo & lhs, const DarknetFileInfo & rhs)
			{
				if (lhs.type < rhs.type)
				{
					return true;
				}
				if (lhs.type == rhs.type)
				{
					// reverse this test so the NEWER files appear at the top prior to the older files
					if (rhs.timestamp < lhs.timestamp)
					{
						return true;
					}

					if (lhs.timestamp == rhs.timestamp)
					{
						return rhs.short_name < lhs.short_name;
					}
				}
				return false;
			} );

	if (hide_some_weight_files.getToggleState())
	{
		filter_out_extra_weight_files();
	}
	else
	{
		applying_filter = false;
		table.updateContent();
	}

	// if we don't have a .cfg, .weights, or .names file to use, then look through the list of files and choose the most recent one
	if (darknet_configuration_filename.toString().isEmpty())
	{
		for (DarknetFileInfo & info : v)
		{
			if (info.type == DarknetFileInfo::EType::kCfg)
			{
				darknet_configuration_filename = info.full_name.c_str();
				break;
			}
		}
	}
	if (darknet_weights_filename.toString().isEmpty())
	{
		for (DarknetFileInfo & info : v)
		{
			if (info.type == DarknetFileInfo::EType::kWeight)
			{
				darknet_weights_filename = info.full_name.c_str();
				break;
			}
		}
	}
	if (darknet_names_filename.toString().isEmpty())
	{
		for (DarknetFileInfo & info : v)
		{
			if (info.type == DarknetFileInfo::EType::kNames)
			{
				darknet_names_filename = info.full_name.c_str();
				break;
			}
		}
	}

	const String fn = darknet_names_filename.toString();
	if (File(fn).existsAsFile())
	{
		VStr v;
		std::ifstream ifs(fn.toStdString());
		std::string line;
		while (std::getline(ifs, line))
		{
			if (line.empty())
			{
				break;
			}
			v.push_back(line);
		}
		number_of_classes = String(v.size());
	}
	else
	{
		number_of_classes = "?";
	}

	return;
}


int dm::StartupCanvas::getNumRows()
{
	if (applying_filter)
	{
		return 1;
	}

	return v.size();
}


void dm::StartupCanvas::paintRowBackground(Graphics & g, int rowNumber, int width, int height, bool rowIsSelected)
{
	if (rowNumber < 0				or
		rowNumber >= (int)v.size()	)
	{
		// rows are 0-based, columns are 1-based
		return;
	}

	Colour colour = Colours::white;
	if (rowIsSelected)
	{
		colour = Colours::lightblue; // selected rows will have a blue background
	}
	else if (applying_filter)
	{
		colour = Colours::lightyellow;
	}
	else
	{
		// does this row match ones of the key selected lines (cfg, .weights, or .names)?
		const DarknetFileInfo & info = v.at(rowNumber);
		const String full_name = info.full_name;
		if (full_name == darknet_configuration_filename	or
			full_name == darknet_weights_filename		or
			full_name == darknet_names_filename			)
		{
			colour = Colours::lightgreen;
		}
	}

	g.fillAll( colour );

	// draw the cell bottom divider between rows
	g.setColour( Colours::black.withAlpha( 0.5f ) );
	g.drawLine( 0, height, width, height );

	return;
}


void dm::StartupCanvas::paintCell(Graphics & g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
	if (rowNumber < 0				or
		rowNumber >= (int)v.size()	or
		columnId < 1				or
		columnId > 4				)
	{
		// rows are 0-based, columns are 1-based
		return;
	}

	std::string str;
	if (applying_filter)
	{
		if (columnId == 1)
		{
			str = "Calculating MD5 checksums...";
		}
	}
	else
	{
		const DarknetFileInfo & info = v.at(rowNumber);

		/* columns:
		 *		1: filename
		 *		2: type
		 *		3: size
		 *		4: timestamp
		 */
		switch (columnId)
		{
			case 2: str = (	info.type == DarknetFileInfo::EType::kCfg		?	"configuration"		:
							info.type == DarknetFileInfo::EType::kWeight	?	"weights"			:
																				"names"				);	break;
			case 1: str = info.short_name;																break;
			case 3: str = info.file_size;																break;
			case 4: str = info.timestamp;																break;
		}
	}

	// draw the text and the right-hand-side dividing line between cells
	g.setColour( Colours::black );
	Rectangle<int> r(0, 0, width, height);
	g.drawFittedText(str, r.reduced(2), Justification::centredLeft, 1 );

	// draw the divider on the right side of the column
	g.setColour( Colours::black.withAlpha( 0.5f ) );
	g.drawLine( width, 0, width, height );

	return;
}


void dm::StartupCanvas::selectedRowsChanged(int rowNumber)
{
	if (rowNumber < 0				or
		rowNumber >= (int)v.size()	)
	{
		// rows are 0-based, columns are 1-based
		return;
	}

	const DarknetFileInfo & info = v.at(rowNumber);

	switch (info.type)
	{
		case DarknetFileInfo::EType::kCfg:
		{
			darknet_configuration_filename = info.full_name.c_str();
			break;
		}
		case DarknetFileInfo::EType::kWeight:
		{
			darknet_weights_filename = info.full_name.c_str();
			break;
		}
		case DarknetFileInfo::EType::kNames:
		{
			darknet_names_filename = info.full_name.c_str();
			break;
		}
	}

	table.repaint();

	return;
}


void dm::StartupCanvas::cellClicked(int rowNumber, int columnId, const MouseEvent & event)
{
	if (event.mods.isPopupMenu())
	{
		PopupMenu m;
		m.addItem("delete unused and duplicate .weights files", (extra_weights_files.size() > 0), false, std::function<void()>( [&]{ delete_extra_weights_files(); }));
		m.showMenuAsync(PopupMenu::Options());
	}

	return;
}


void dm::StartupCanvas::delete_extra_weights_files()
{
	for (const auto & filename : extra_weights_files)
	{
		Log("deleting extra weights file: " + filename);
		File(filename).deleteFile();
	}

	if (extra_weights_files.empty() == false)
	{
		const size_t count = extra_weights_files.size();
		AlertWindow::showMessageBox(AlertWindow::AlertIconType::InfoIcon, "DarkMark", "Deleted " + std::to_string(count) + " unused or duplicate .weights file" + (count == 1 ? "" : "s") + " from " + project_directory.toString().toStdString() + ".");
		extra_weights_files.clear();
	}

	return;
}


void dm::StartupCanvas::valueChanged(Value & value)
{
	table.repaint();

	return;
}


void dm::StartupCanvas::buttonClicked(Button * button)
{
	if (button == &hide_some_weight_files)
	{
		// this can take a while, so start it on a thread
		// (and in the case where this toggle is set to FALSE, we have no way to get back the entries we deleted)
		done = false;
		t.join();
		t = std::thread(&StartupCanvas::find_all_darknet_files, this);
	}

	return;
}


void dm::StartupCanvas::filter_out_extra_weight_files()
{
	/* Darknet spits out many .weights files when learning ends, such as:
	 *
	 *		mailboxes_yolov3-tiny_40000.weights
	 *		mailboxes_yolov3-tiny_final.weights
	 *		mailboxes_yolov3-tiny_best.weights
	 *		mailboxes_yolov3-tiny_last.weights
	 *
	 * ...all of which may actually be exactly the same.  We want to filter out duplicates.
	 * So we'll store the MD5 hash of each .weights file, that way we'll know if this is
	 * a duplicate.
	 */
	SStr md5s;
	extra_weights_files.clear();

	VDarknetFileInfo new_files;

	applying_filter = true;
	table.updateContent();

	for (auto & info : v)
	{
		if (done)
		{
			break;
		}

		if (info.type != DarknetFileInfo::EType::kWeight)
		{
			new_files.push_back(info);
			continue;
		}

		// otherwise if we get here then we have a .weights file, so check to see if this is one we want to keep

		if (md5s.empty()													or
			info.short_name.find("_best.weights")	!= std::string::npos	or
			info.short_name.find("_last.weights")	!= std::string::npos	or
			info.short_name.find("_final.weights")	!= std::string::npos	)
		{
			Log("calculating MD5 checksum for " + info.full_name);
			const std::string md5 = MD5(File(info.full_name)).toHexString().toStdString();
			if (md5s.count(md5) == 0)
			{
				new_files.push_back(info);
				md5s.insert(md5);
			}
			else
			{
				extra_weights_files.insert(info.full_name);
			}
		}
		else
		{
			extra_weights_files.insert(info.full_name);
		}
	}

	applying_filter = false;

	v.swap(new_files);
	table.updateContent();

	Log(project_directory.toString().toStdString() + ": number of .weights files to skip: " + std::to_string(extra_weights_files.size()));

	return;
}
