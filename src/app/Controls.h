/*
 * Quill - small controls with the look of Word 5.1's bars: 3D icon
 * buttons, pop-up fields and the page number box.
 */
#ifndef QUILL_CONTROLS_H
#define QUILL_CONTROLS_H

#include <Control.h>
#include <String.h>
#include <View.h>

#include <string>
#include <vector>

class BTextControl;

class IconButton : public BControl {
public:
							IconButton(const char* name, int icon,
								BMessage* message, const char* toolTip,
								bool toggle = false);
							IconButton(const char* name, const char* text,
								uint16 face, BMessage* message,
								const char* toolTip, bool toggle = false);

	virtual	void			Draw(BRect updateRect);
	virtual	void			MouseDown(BPoint where);
	virtual	void			MouseMoved(BPoint where, uint32 transit,
								const BMessage* dragMessage);
	virtual	void			MouseUp(BPoint where);
	virtual	BSize			MinSize();
	virtual	BSize			MaxSize();
	virtual	BSize			PreferredSize();

			void			SetOn(bool on);

private:
			int				fIcon;
			BString			fText;
			uint16			fFace;
			bool			fToggle;
			bool			fTracking;
			bool			fInside;
};

class PopupField : public BView {
public:
							PopupField(const char* name, BMessage* message,
								const char* field, float width,
								const char* toolTip);
	virtual					~PopupField();

	virtual	void			Draw(BRect updateRect);
	virtual	void			MouseDown(BPoint where);
	virtual	BSize			MinSize();
	virtual	BSize			MaxSize();
	virtual	BSize			PreferredSize();

			void			SetItems(const std::vector<std::string>& items);
			void			SetText(const char* text);
			const char*		Text() const { return fText.String(); }
			void			SetTarget(BHandler* target) { fTarget = target; }
			void			SetExtraItem(const char* label, BMessage* message);

private:
			BMessage*		fMessage;
			BString			fField;
			BString			fText;
			float			fWidth;
			std::vector<std::string> fItems;
			BHandler*		fTarget;
			BString			fExtraLabel;
			BMessage*		fExtraMessage;
};

// The font size box of the Ribbon: type a size or pick one.
class SizeField : public BView {
public:
							SizeField(const char* name, BMessage* message);

	virtual	void			AttachedToWindow();
	virtual	void			MessageReceived(BMessage* message);

			void			SetSize(float size);	// 0: mixed
			void			SetTarget(BHandler* target);

private:
			BTextControl*	fText;
			PopupField*		fPopup;
			BMessage*		fMessage;
			BHandler*		fTarget;
};

class StatusView : public BView {
public:
							StatusView(const char* name);

	virtual	void			Draw(BRect updateRect);
	virtual	BSize			MinSize();
	virtual	BSize			MaxSize();
	virtual	BSize			PreferredSize();
	virtual	void			MouseDown(BPoint where);

			void			SetText(const char* text);
			void			SetMessage(const char* text);	// temporary
			const char*		Text() const { return fText.String(); }
			const char*		Message() const { return fMessage.String(); }

private:
			BString			fText;
			BString			fMessage;
};

class SeparatorBar : public BView {
public:
							SeparatorBar(float width = 8);
	virtual	void			Draw(BRect updateRect);
};

rgb_color BarColor();

#endif	// QUILL_CONTROLS_H
