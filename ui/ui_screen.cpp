#include "ui/ui_screen.h"
#include "ui/ui_context.h"
#include "ui/screen.h"

UIScreen::UIScreen()
	: Screen(), root_(0), recreateViews_(true) {
}

void UIScreen::DoRecreateViews() {
	if (recreateViews_) {
		delete root_;
		root_ = 0;
		CreateViews();
		recreateViews_ = false;
	}
}

void UIScreen::update(InputState &input) {
	DoRecreateViews();

	if (root_) {
		UpdateViewHierarchy(input, root_);
	} else {
		ELOG("Tried to update without a view root");
	}
}

void UIScreen::render() {
	DoRecreateViews();

	if (root_) {
		UI::LayoutViewHierarchy(*screenManager()->getUIContext(), root_);

		screenManager()->getUIContext()->Begin();
		DrawBackground(*screenManager()->getUIContext());
		root_->Draw(*screenManager()->getUIContext());
		screenManager()->getUIContext()->End();
		screenManager()->getUIContext()->Flush();
	} else {
		ELOG("Tried to render without a view root");
	}
}

void UIScreen::touch(const TouchInput &touch) {
	if (root_) {
		root_->Touch(touch);
	} else {
		ELOG("Tried to touch without a view root");
	}
}

void UIScreen::key(const KeyInput &key) {
	if (root_) {
		root_->Key(key);
	} else {
		ELOG("Tried to key without a view root");
	}
}

UI::EventReturn UIScreen::OnBack(UI::EventParams &e) {
	screenManager()->finishDialog(this, DR_OK);
	return UI::EVENT_DONE;
}


PopupScreen::PopupScreen(const std::string &title)
	: title_(title) {}

void PopupScreen::CreateViews() {
	using namespace UI;

	root_ = new AnchorLayout(new LayoutParams(FILL_PARENT, FILL_PARENT));
	LinearLayout *box = new LinearLayout(ORIENT_VERTICAL, new AnchorLayoutParams(50, 20, 50, 80)); 

	root_->Add(box);
	box->SetBG(UI::Drawable(0x60303030));
	box->SetHasDropShadow(true);

	View *title = new ItemHeader(title_);
	box->Add(title);

	CreatePopupContents(box);

	// And the two buttons at the bottom.
	ViewGroup *buttonRow = new LinearLayout(ORIENT_HORIZONTAL);
	buttonRow->Add(new Button("OK", new LinearLayoutParams(1.0f)))->OnClick.Handle(this, &PopupScreen::OnOK);
	buttonRow->Add(new Button("Cancel", new LinearLayoutParams(1.0f)))->OnClick.Handle(this, &PopupScreen::OnCancel);
	box->Add(buttonRow);
}

UI::EventReturn PopupScreen::OnOK(UI::EventParams &e) {
	OnCompleted();
	screenManager()->finishDialog(this, DR_OK);
	return UI::EVENT_DONE;
}

UI::EventReturn PopupScreen::OnCancel(UI::EventParams &e) {
	screenManager()->finishDialog(this, DR_CANCEL);
	return UI::EVENT_DONE;
}


void ListPopupScreen::CreatePopupContents(UI::ViewGroup *parent) {
	using namespace UI;

	listView_ = parent->Add(new ListView(&adaptor_, new LinearLayoutParams(1.0)));
	listView_->OnChoice.Handle(this, &ListPopupScreen::OnListChoice);
}

UI::EventReturn ListPopupScreen::OnListChoice(UI::EventParams &e) {
	adaptor_.SetSelected(e.a);
	OnChoice.Dispatch(e);
	return UI::EVENT_DONE;
}

void ListPopupScreen::OnCompleted() {
	callback_(adaptor_.GetSelected());	
}

void SliderPopupScreen::CreatePopupContents(UI::ViewGroup *parent) {
	using namespace UI;
	slider_ = parent->Add(new Slider(value_, minValue_, maxValue_));
}

void SliderFloatPopupScreen::CreatePopupContents(UI::ViewGroup *parent) {
	using namespace UI;
	slider_ = parent->Add(new SliderFloat(value_, minValue_, maxValue_));
}
