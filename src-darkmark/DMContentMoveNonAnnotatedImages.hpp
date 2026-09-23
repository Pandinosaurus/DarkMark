// DarkMark (C) 2019-2026 Stephane Charette <stephanecharette@gmail.com>

#pragma once

#include "DarkMark.hpp"


namespace dm
{
	class DMContentMoveNonAnnotatedImages : public ThreadWithProgressWindow
	{
		public:

			DMContentMoveNonAnnotatedImages(dm::DMContent & c);

			virtual ~DMContentMoveNonAnnotatedImages();

			virtual void run();

			DMContent & content;
	};
}
