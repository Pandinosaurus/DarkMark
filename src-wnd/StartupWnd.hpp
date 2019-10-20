/* DarkMark (C) 2019 Stephane Charette <stephanecharette@gmail.com>
 * $Id$
 */

#pragma once

#include "DarkMark.hpp"


namespace dm
{
	class StartupWnd : public DocumentWindow, public Button::Listener
	{
		public:

			StartupWnd();

			virtual ~StartupWnd();

			virtual void closeButtonPressed();
			virtual void userTriedToCloseWindow();

			virtual void resized();

			virtual void buttonClicked(Button * button);

			virtual bool keyPressed(const KeyPress & key);

			Component canvas;
			Notebook notebook;
			TextButton add_button;
			TextButton delete_button;
			TextButton ok_button;
			TextButton cancel_button;
	};
}
