#pragma once

enum class SectionID : int;

void GotoSection(SectionID sectionID, void* screen_data = nullptr);
void ChangeNavigationSection(SectionID sectionID);
bool MoveOfficerCanvas(bool goLeft);
