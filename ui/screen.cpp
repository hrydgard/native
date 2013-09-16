#include "base/logging.h"
#include "input/input_state.h"
#include "ui/screen.h"
#include "ui/ui.h"

ScreenManager::ScreenManager() {
	nextScreen_ = 0;
	uiContext_ = 0;
	dialogFinished_ = 0;
}

ScreenManager::~ScreenManager() {
	shutdown();
}

void ScreenManager::switchScreen(Screen *screen) {
	// Note that if a dialog is found, this will be a silent background switch that
	// will only become apparent if the dialog is closed. The previous screen will stick around
	// until that switch.
	// TODO: is this still true?
	if (nextScreen_ != 0) {
		FLOG("WTF? Already had a nextScreen_");
	}
	if (screen == 0) {
		WLOG("Swiching to a zero screen, this can't be good");
	}
	if (stack_.empty() || screen != stack_.back().screen) {
		nextScreen_ = screen;
		nextScreen_->setScreenManager(this);
	}
}

void ScreenManager::update(InputState &input) {
	if (nextScreen_) {
		switchToNext();
	}

	if (stack_.size()) {
		stack_.back().screen->update(input);
	}
}

void ScreenManager::switchToNext() {
	if (!nextScreen_) {
		ELOG("switchToNext: No nextScreen_!");
	}

	Layer temp = {0, 0};
	if (!stack_.empty()) {
		temp = stack_.back();
		stack_.pop_back();
	}
	Layer newLayer = {nextScreen_, 0};
	stack_.push_back(newLayer);
	if (temp.screen) {
		delete temp.screen;
	}
	nextScreen_ = 0;
}

void ScreenManager::touch(const TouchInput &touch) {
	if (!stack_.empty())
		stack_.back().screen->touch(touch);
}

void ScreenManager::key(const KeyInput &key) {
	if (!stack_.empty())
		stack_.back().screen->key(key);
}

void ScreenManager::axis(const AxisInput &axis) {
	if (!stack_.empty())
		stack_.back().screen->axis(axis);
}


void ScreenManager::render() {
	if (!stack_.empty()) {
		switch (stack_.back().flags) {
		case LAYER_SIDEMENU:
		case LAYER_TRANSPARENT:
			if (stack_.size() == 1) {
				ELOG("Can't have sidemenu over nothing");
				break;
			} else {
				auto iter = stack_.end();
				iter--;
				iter--;
				Layer backback = *iter;
				UIDisableBegin();
				// Also shift to the right somehow...
				backback.screen->render();
				UIDisableEnd();
				stack_.back().screen->render();
				break;
			}
		default:
			stack_.back().screen->render();
			break;
		}
	} else {
		ELOG("No current screen!");
	}

	processFinishDialog();
}

void ScreenManager::sendMessage(const char *msg, const char *value) {
	if (!stack_.empty())
		stack_.back().screen->sendMessage(msg, value);
}

void ScreenManager::deviceLost() {
	for (size_t i = 0; i < stack_.size(); i++) {
		stack_[i].screen->deviceLost();
	}
	// Dialogs too? Nah, they should only use the standard UI texture anyway.
	// TODO: Change this when it becomes necessary.
}

Screen *ScreenManager::topScreen() const {
	if (!stack_.empty())
		return stack_.back().screen;
	else
		return 0;
}

void ScreenManager::shutdown() {
	for (auto x = stack_.begin(); x != stack_.end(); x++)
		delete x->screen;
	stack_.clear();
	delete nextScreen_;
	nextScreen_ = 0;
}

void ScreenManager::push(Screen *screen, int layerFlags) {
	if (nextScreen_ && stack_.empty()) {
		// we're during init, this is OK
		switchToNext();
	}
	screen->setScreenManager(this);
	if (screen->isTransparent()) {
		layerFlags |= LAYER_TRANSPARENT;
	}
	Layer layer = {screen, layerFlags};
	stack_.push_back(layer);
}

void ScreenManager::pop() {
	if (stack_.size()) {
		delete stack_.back().screen;
		stack_.pop_back();
	} else {
		ELOG("Can't pop when stack empty");
	}
}

void ScreenManager::RecreateAllViews() {
	for (auto it = stack_.begin(); it != stack_.end(); ++it) {
		it->screen->RecreateViews();
	}
}

void ScreenManager::finishDialog(const Screen *dialog, DialogResult result) {
	if (stack_.empty()) {
		ELOG("Must be in a dialog to finishDialog");
		return;
	}
	if (dialog != stack_.back().screen) {
		ELOG("Wrong dialog being finished!");
		return;
	}
	dialogFinished_ = dialog;
	dialogResult_ = result;
}

void ScreenManager::processFinishDialog() {
	if (dialogFinished_) {
		// Another dialog may have been pushed before the render, so search for it.
		Screen *caller = 0;
		for (size_t i = 0; i < stack_.size(); ++i) {
			if (stack_[i].screen != dialogFinished_) {
				continue;
			}

			stack_.erase(stack_.begin() + i);
			// The previous screen was the caller (not necessarily the topmost.)
			if (i > 0) {
				caller = stack_[i - 1].screen;
			}
		}

		if (!caller) {
			ELOG("ERROR: no top screen when finishing dialog");
		} else if (caller != topScreen()) {
			// The caller may get confused if we call dialogFinished() now.
			WLOG("Skipping non-top dialog when finishing dialog.");
		} else {
			caller->dialogFinished(dialogFinished_, dialogResult_);
		}
		delete dialogFinished_;
		dialogFinished_ = 0;
	}
}
