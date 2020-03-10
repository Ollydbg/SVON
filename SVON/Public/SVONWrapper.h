#pragma once

#include "SVONVolume.h"

namespace SVON
{
	class SVON_API SVONWrapper
	{
	public:
		static SVONWrapper* GetInstance()
		{
			if (instance == nullptr)
			{
				instance = new SVONWrapper();
			}
			return instance;
		}

		SVONVolume* CreateSVONVolume(GetVolumBoudingBoxFunc getVolumBoudingBoxFunc,
			OverlapBoxBlockingTestFunc boxOverlapCheckFunc);
		void ReleaseSVONVolume(SVONVolume* vol);
		bool SVONVolumeGenerate(SVONVolume* vol);

	private:
		static SVONWrapper* instance;
	};
}
