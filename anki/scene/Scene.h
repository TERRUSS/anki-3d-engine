#ifndef ANKI_SCENE_SCENE_H
#define ANKI_SCENE_SCENE_H

#include "anki/scene/SceneNode.h"
#include "anki/math/Math.h"
#include <boost/range/iterator_range.hpp>
#include <vector>


namespace anki {


/// The Scene contains all the dynamic entities
///
/// XXX Add physics
class Scene
{
public:
	template<typename T>
	struct Types
	{
		typedef std::vector<T*> Container;
		typedef typename Container::iterator Iterator;
		typedef typename Container::const_iterator ConstIterator;
		typedef boost::iterator_range<Iterator> MutableRange;
		typedef boost::iterator_range<ConstIterator> ConstRange;
		typedef typename ConstCharPtrHashMap<T*>::Type NameToItemMap;
	};

	/// @name Constructors/Destructor
	/// @{
	Scene();
	~Scene();
	/// @}

	/// Put a node in the appropriate containers
	void registerNode(SceneNode* node);
	void unregisterNode(SceneNode* node);

	void update(float prevUpdateTime, float crntTime, int frame);

	/*void doVisibilityTests(Camera& cam)
	{
		//XXX visibilityTester->test(cam);
	}*/

	/// @name Accessors
	/// @{
	const Vec3& getAmbientColor() const
	{
		return ambientCol;
	}
	Vec3& getAmbientColor()
	{
		return ambientCol;
	}
	void setAmbientColor(const Vec3& x)
	{
		ambientCol = x;
	}

	Types<SceneNode>::ConstRange getAllNodes() const
	{
		return Types<SceneNode>::ConstRange(nodes.begin(), nodes.end());
	}
	Types<SceneNode>::MutableRange getAllNodes()
	{
		return Types<SceneNode>::MutableRange(nodes.begin(), nodes.end());
	}
	/// @}

private:
	/// @name Containers of scene's data
	/// @{
	Types<SceneNode>::Container nodes;
	/*Types<Movable>::Container movables;
	Types<Renderable>::Container renderables;
	Types<Spatial>::Container spatials;
	Types<Frustumable>::Container frustumables;*/
	/// @}

	Types<SceneNode>::NameToItemMap nameToNode;

	Vec3 ambientCol; ///< The global ambient color

	/// XXX
	template<typename T>
	void addC(typename Types<T>::Container& c, T* ptr)
	{
		ANKI_ASSERT(std::find(c.begin(), c.end(), ptr) == c.end());
		c.push_back(ptr);
	}

	/// XXX
	template<typename T>
	void addDict(typename Types<T>::NameToItemMap& d, T* ptr)
	{
		if(d.find(ptr->getName().c_str()) != d.end())
		{
			throw ANKI_EXCEPTION("Item already exists: " + ptr->getName());
		}

		d[ptr->getName().c_str()] = ptr;
	}

	/// XXX
	template<typename T>
	void removeC(typename Types<T>::Container& c, T* ptr)
	{
		typename Types<T>::Iterator it = c.begin();
		for(; it != c.end(); ++it)
		{
			if(*it == ptr)
			{
				break;
			}
		}

		ANKI_ASSERT(it != c.end());
		c.erase(it);
	}

	/// XXX
	template<typename T>
	void removeDict(typename Types<T>::NameToItemMap& d, T* ptr)
	{
		typename Types<T>::NameToItemMap::iterator it =
			d.find(ptr->getName().c_str());

		ANKI_ASSERT(it != d.end());
		d.erase(it);
	}
};


} // end namespace


#endif
