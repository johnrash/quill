/*
 * Quill - storing formats in BMessages (dialog results, scripting).
 */
#ifndef QUILL_FORMAT_MESSAGES_H
#define QUILL_FORMAT_MESSAGES_H

#include "Format.h"

#include <Message.h>

void AddCharFormat(BMessage* message, const quill::CharFormat& format);
void FindCharFormat(const BMessage* message, quill::CharFormat* format);
void AddParaFormat(BMessage* message, const quill::ParaFormat& format);
void FindParaFormat(const BMessage* message, quill::ParaFormat* format);
void AddBorder(BMessage* message, const quill::ParaBorder& border);
void FindBorder(const BMessage* message, quill::ParaBorder* border);
void AddTabs(BMessage* message, const std::vector<quill::TabStop>& tabs);
void FindTabs(const BMessage* message, std::vector<quill::TabStop>* tabs);
void AddSettings(BMessage* message, const quill::DocSettings& settings);
void FindSettings(const BMessage* message, quill::DocSettings* settings);

#endif	// QUILL_FORMAT_MESSAGES_H
