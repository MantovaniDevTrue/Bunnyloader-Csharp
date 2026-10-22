#pragma once
namespace bl::csharp {
void managedUndergroundDispatch(void(*dispatch)());
void removeManagedUndergroundDispatch();
}
