/*
 * BoundingSphere.h
 *
 *  Created on: Jul 6, 2014
 *      Author: sam
 */

#ifndef BOUNDINGSPHERE_H_
#define BOUNDINGSPHERE_H_
#include "../Polygon.h"
#include <vector>

template<unsigned int D,class T>
class AbstractBoundingSphere:public BoundingVolume<D,T>{
protected:
	Point<D,T> circleCenter;
	T circleRadius;
public:
	AbstractBoundingSphere(){

	}
	virtual ~AbstractBoundingSphere(){

	}
	ShapeType getType(){
		return BOUNDING_SPHERE;
	}
	virtual void update()=0;
	bool contains(const Point<D,T> & point, bool inclusive=true){
		if(inclusive){
			return point.distance_underapprox(circleCenter)<=circleRadius;
		}else{
			return point.distance_underapprox(circleCenter)<circleRadius;
		}
	}
	bool intersects(Shape<D,T> & s, bool inclusive=true){
		assert(false);//not yet implemented
		return false;
	}
};

template<unsigned int D,class T, class Bound>
class BoundingSphere:public AbstractBoundingSphere<D,T>{

	Bound & toBound;
public:
	BoundingSphere(Bound & toBound):toBound(toBound){

	}
	ShapeType getType(){
		return BOUNDING_SPHERE;
	}
	void update();

};
template<unsigned int D,class T>
class BoundingSphere<D,T,Polygon<D,T>>:public AbstractBoundingSphere<D,T>{
	Point<D,T> circleCenter;
	T circleRadius;
	Polygon<D,T> & toBound;
public:
	BoundingSphere(Bound & toBound):toBound(toBound){

	}
	ShapeType getType(){
		return BOUNDING_SPHERE;
	}
	void update(){
		circleCenter.zero();
		std::vector<Point<D,T>> & vertices  = toBound.getVertices();

		for(int i = 0;i<vertices.size();i++){
			circleCenter+=vertices[i];
		}
		circleCenter/=T(vertices.size());
		circleRadius=T(0);
		for(int i = 0;i<vertices.size();i++){
			T dist = circleCenter.distance( vertices[i]);
			if(dist>circleRadius){
				circleRadius=dist;
			}
		}
	}

};


#endif /* BOUNDINGSPHERE_H_ */