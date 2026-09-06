#include <OgreRmlUi/Shaders.h>

#include <OgreRmlUi/Utils.h>

using namespace OgreRmlUi;

GradientMaker::GradientMaker(GradientMaker::Type type) :
	type{type}
{
	this->baseMaterial = Ogre::MaterialManager::getSingleton().getByName("Rml/Gradient");
}

Ogre::MaterialPtr GradientMaker::makeGradient(
	const Rml::Dictionary& parameters,
	Ogre::Vector2& origin,
	Ogre::Vector2& scale)
{
	auto material = this->baseMaterial->clone("");
	material->load();
	auto fragmentProgramParameters = material
		->getBestTechnique()
		->getPass(0)
		->getFragmentProgramParameters();

	fragmentProgramParameters->setNamedConstant("gradientType", int(this->type));
	fragmentProgramParameters->setNamedConstant("repeating", int(Rml::Get(parameters, "repeating", false)));
	fragmentProgramParameters->setNamedConstant("origin", origin);
	fragmentProgramParameters->setNamedConstant("scale", scale);

	auto stopListIter = parameters.find("color_stop_list");
	assert(stopListIter  != parameters.end()
		&& stopListIter ->second.GetType() == Rml::Variant::COLORSTOPLIST);
	const Rml::ColorStopList& stopList = stopListIter->second.GetReference<Rml::ColorStopList>();
	int stopListSize = (int)stopList.size();

	std::array<float, MAX_STOP_COLOURS> stopPositions;
	std::array<float, MAX_STOP_COLOURS * 4> stopColours;
	for (int i = 0, j = 0; i < stopListSize; ++i)
	{
		stopPositions[i] = stopList[i].position.number;
		auto colour = util::toOgre(stopList[i].color);
		for (int k = 0; k < 4; ++k)
			stopColours[j++] = colour[k];
	}

	fragmentProgramParameters->setNamedConstant("numStops", stopListSize);
	fragmentProgramParameters->setNamedConstant(
		"stopPositions",
		stopPositions.data(),
		4);
	fragmentProgramParameters->setNamedConstant(
		"stopColours",
		stopColours.data(),
		stopColours.size() / 4);

	return material;
}


Ogre::MaterialPtr LinearGradientMaker::make(const Rml::Dictionary& parameters)
{
	auto p0 = Rml::Get(parameters, "p0", Rml::Vector2f(0.0f));
	auto p1 = Rml::Get(parameters, "p1", Rml::Vector2f(0.0f));
	auto origin = util::toOgre(p0);
	auto scale = util::toOgre(p1) - origin;
	return this->makeGradient(parameters, origin, scale);
}

Ogre::MaterialPtr RadialGradientMaker::make(const Rml::Dictionary& parameters)
{
	auto center = Rml::Get(parameters, "center", Rml::Vector2f(0.0f));
	auto radius = Rml::Get(parameters, "radius", Rml::Vector2f(1.0f));
	auto origin = util::toOgre(center);
	auto scale = Ogre::Vector2(1.0f) / util::toOgre(radius);
	return this->makeGradient(parameters, origin, scale);
}

Ogre::MaterialPtr ConicGradientMaker::make(const Rml::Dictionary& parameters)
{
	auto center = Rml::Get(parameters, "center", Rml::Vector2f(0.0f));
	auto angle = Rml::Get(parameters, "angle", 0.0f);
	auto origin = util::toOgre(center);
	auto scale = Ogre::Vector2{Ogre::Math::Cos(angle), Ogre::Math::Sin(angle)};
	return this->makeGradient(parameters, origin, scale);
}
