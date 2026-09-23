// DarkMark (C) 2019-2026 Stephane Charette <stephanecharette@gmail.com>

#include "DarkMark.hpp"


dm::DMContentMoveNonAnnotatedImages::DMContentMoveNonAnnotatedImages(dm::DMContent & c) :
	ThreadWithProgressWindow("Moving non-annotated images...", true, true),
	content(c)
{
	return;
}


dm::DMContentMoveNonAnnotatedImages::~DMContentMoveNonAnnotatedImages()
{
	return;
}


void dm::DMContentMoveNonAnnotatedImages::run()
{
	DarkMarkApplication::setup_signal_handling();

	const auto previous_scrollfield_width = content.scrollfield_width;
	if (previous_scrollfield_width > 0)
	{
		content.scrollfield_width = 0;
		content.resized();
	}

	// disable all predictions
//	const auto previous_predictions = content.show_predictions;
//	content.show_predictions = EToggle::kOff;

	File dir = File(content.project_info.project_dir).getChildFile("non-annotated_images");
	dir.createDirectory();

	const double max_work = content.image_filenames.size();
	double work_completed = 0.0;

	for (size_t idx = 0; idx < content.image_filenames.size(); idx ++)
	{
		if (threadShouldExit())
		{
			break;
		}

		setProgress(work_completed / max_work);
		work_completed ++;

		std::string & fn = content.image_filenames.at(idx);
		File f1 = File(fn);

		if (f1.isAChildOf(dir))
		{
			// this file is already in the "non-annotated images" folder
			continue;
		}

		File f2 = f1.withFileExtension(".json");
		File f3 = f1.withFileExtension(".txt");
		if (f2.existsAsFile() or f3.existsAsFile())
		{
			// we found annotations
			continue;
		}

		File f4 = dir.getChildFile(f1.getFileName());

		Log("moving " + f1.getFullPathName().toStdString() + " to " + f4.getFullPathName().toStdString());

		f1.moveFileTo(f4);

		fn = f4.getFullPathName().toStdString();
	}

	content.scrollfield_width = previous_scrollfield_width;
	content.scrollfield.rebuild_entire_field_on_thread();

	return;
}
