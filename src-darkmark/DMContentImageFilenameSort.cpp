// DarkMark (C) 2019-2024 Stephane Charette <stephanecharette@gmail.com>

#include "DarkMark.hpp"

#include "json.hpp"
using json = nlohmann::json;


dm::DMContentImageFilenameSort::DMContentImageFilenameSort(dm::DMContent & c) :
		ThreadWithProgressWindow("Sorting images...", true, true),
		content(c)
{
	return;
}


void dm::DMContentImageFilenameSort::run()
{
	DarkMarkApplication::setup_signal_handling();

	if (content.sort_order != dm::ESort::kSimilarMarks					and
		content.sort_order != dm::ESort::kCountMarks					and
		content.sort_order != dm::ESort::kTimestamp						and
		content.sort_order != dm::ESort::kMinimumIoU					and
		content.sort_order != dm::ESort::kAverageIoU					and
		content.sort_order != dm::ESort::kMaximumIoU					and
		content.sort_order != dm::ESort::kNumberOfPredictions			and
		content.sort_order != dm::ESort::kNumberOfDifferences			and
		content.sort_order != dm::ESort::kPredictionsWithoutAnnotations	and
		content.sort_order != dm::ESort::kAnnotationsWithoutPredictions	)
	{
		// this class can only sort those few types, everything else should be handled directly in DMContent::set_sort_order()
		Log("ERROR: progress thread cannot sort for type #" + std::to_string((int)content.sort_order));

		return;
	}

	// first thing we need to do is sort alphabetically so we can maintain a predictable order between images (issue #38)
	std::sort(content.image_filenames.begin(), content.image_filenames.end());

	std::map<std::string, float> m;

	const std::time_t now = std::time(nullptr);

	const double max_work = content.image_filenames.size();
	double work_completed = 0.0;

	for (const auto & fn : content.image_filenames)
	{
		if (threadShouldExit())
		{
			break;
		}

		setProgress(work_completed / max_work);
		work_completed ++;

		File file = File(fn).withFileExtension(".json");
		if (content.sort_order == dm::ESort::kCountMarks)
		{
			m[fn] = content.count_marks_in_json(file, true);
		}
		else if (content.sort_order == dm::ESort::kSimilarMarks)
		{
			m[fn] = content.build_id_from_classes(file);
		}
		else
		{
			// if we get here, then we need to get something from the .json file
			json j;
			if (file.existsAsFile())
			{
				j = json::parse(file.loadFileAsString().toStdString());
			}

			if (content.sort_order == dm::ESort::kMinimumIoU)
			{
				float f = 0.0f;
				if (j.contains("predictions"))
				{
					f = j["predictions"]["IoU"]["min"];
				}
				m[fn] = f;
			}
			else if (content.sort_order == dm::ESort::kAverageIoU)
			{
				float f = 0.0f;
				if (j.contains("predictions"))
				{
					f = j["predictions"]["IoU"]["avg"];
				}
				m[fn] = f;
			}
			else if (content.sort_order == dm::ESort::kMaximumIoU)
			{
				float f = 0.0f;
				if (j.contains("predictions"))
				{
					f = j["predictions"]["IoU"]["max"];
				}
				m[fn] = f;
			}
			else if (content.sort_order == dm::ESort::kNumberOfPredictions)
			{
				float f = 0.0f;
				if (j.contains("predictions"))
				{
					f = j["predictions"]["count"];
				}
				m[fn] = f;
			}
			else if (content.sort_order == dm::ESort::kNumberOfDifferences)
			{
				float f = 0.0f;
				if (j.contains("predictions"))
				{
					f = j["predictions"]["number_of_differences"];
				}
				m[fn] = f;
			}
			else if (content.sort_order == dm::ESort::kPredictionsWithoutAnnotations)
			{
				float f = 0.0f;
				if (j.contains("predictions"))
				{
					f = j["predictions"]["predictions_without_annotations"];
				}
				m[fn] = f;
			}
			else if (content.sort_order == dm::ESort::kAnnotationsWithoutPredictions)
			{
				float f = 0.0f;
				if (j.contains("predictions"))
				{
					f = j["predictions"]["annotations_without_predictions"];
				}
				m[fn] = f;
			}
			else if (content.sort_order == dm::ESort::kTimestamp)
			{
				/* This is what would be most useful:
					*
					*		1) unmarked images are sorted first
					*		2) marked images are then appended with the most recent image appearing at the very end
					*
					* This way users can press "END" and move LEFT to iterate over images, or press "HOME" and move RIGHT to see
					* unmarked images.
					*
					* The way we do this is to calculate the age of an image:  NOW - time when an image was last modified.  A picture
					* with an age of 1 second was just modified, while a picture with an age of 12345 was modified a long time ago.
					* And to make this work so unmarked images are sorted first, we bias them as being much older than all marked
					* images.  Simply subtract a constant value from the timestamp of unmarked images.
					*/
				size_t timestamp = 0;

				if (j.contains("timestamp"))
				{
					timestamp = now - j["timestamp"].get<std::time_t>();
				}

				// If we don't have a timestamp, then use the file's modification time instead,
				// but subtract a known value so we separate the files with tags and those without.
				if (timestamp == 0)
				{
					timestamp = now - (file.getLastModificationTime().toMilliseconds() / 1000 - 123456789);
				}

				m[fn] = timestamp;
			}
		}
	}

	if (threadShouldExit() == false)
	{
		// now that we've built up the map, we can sort the images (this part is typically very quick)

		setProgress(0.0);

		std::stable_sort(content.image_filenames.begin(), content.image_filenames.end(),
					[&](const auto & lhs, const auto & rhs)
					{
						if (threadShouldExit())
						{
							// abort the sort ASAP
							return false;
						}

						setProgress(work_completed / max_work);
						work_completed ++;

						if (content.sort_order != dm::ESort::kTimestamp)
						{
							return m.at(lhs) < m.at(rhs);
						}

						// otherwise if we get here then we're sorting by timestamp

						// reverse the sort (RHS < LHS versus LHS < RHS) to get the older pictures first, and the newest pictures at the end of the list
						return m.at(rhs) < m.at(lhs);
					} );

#if 0
		for (const auto & fn : content.image_filenames)
		{
			std::cout << fn << " -> " << m[fn] << std::endl;
		}
#endif
	}

	if (threadShouldExit())
	{
		// user hit the "cancel" button, so go back to a normal alphabetical sort order
		content.set_sort_order(dm::ESort::kAlphabetical);
	}

	return;
}
