// UNPAID: OVERTIME - sound id table.

#include "UOSoundSet.h"
#include "Sound/SoundBase.h"

const FUOSoundEntry* UUOSoundSet::FindEntry(FName Id) const
{
	return Entries.FindByPredicate([Id](const FUOSoundEntry& Entry) { return Entry.Id == Id; });
}

int32 UUOSoundSet::CountVariants() const
{
	int32 Count = 0;
	for (const FUOSoundEntry& Entry : Entries)
	{
		Count += Entry.Variants.Num();
	}
	return Count;
}
