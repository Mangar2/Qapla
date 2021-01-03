#ifndef __ATTACKTEST_H
#define __ATTACKTEST_H

#include "../basics/types.h"

const Square KING_DIR[8] = {NORTH, EAST, SOUTH, WEST, NE, SE, SW, NW};
const Square KNIGHT_DIR[8] = {
	WEST + 2 * NORTH, 
	WEST + 2 * SOUTH, 
	EAST + 2 * NORTH, 
	EAST + 2 * SOUTH, 
	WEST * 2 +  NORTH, 
	EAST * 2 + NORTH, 
	WEST * 2 + SOUTH, 
	EAST * 2 + SOUTH
};


class Distance {

public:
	Distance(Square pos) {
		File file = getFile(pos);
		Rank rank = getRank(pos);
		left = DistanceToBorderLeft(xPos);
		right = DistanceToBorderRight(xPos);
		up = DistanceToBorderUp(yPos);
		down = DistanceToBorderDown(yPos);
	}

	Square getDistanceToBorderLeft() const {return left;}
	Square getDistanceToBorderRight() const {return right;}
	Square getDistanceToBorderUp() const {return up;}
	Square getDistanceToBorderDown() const {return down;}

	Square getDistanceToBorderLeftUp() const { return min(left, up); }
	Square getDistanceToBorderLeftDown() const { return min(left, down); }
	Square getDistanceToBorderRightUp() const { return min(right, up); }
	Square getDistanceToBorderRightDown() const { return min(right, down); }

	void fillDistance() {
		distance[0] = up;
		distance[1] = right;
		distance[2] = down;
		distance[3] = left;
		distance[4] = min(right, up);
		distance[5] = min(right, down);
		distance[6] = min(left, down);
		distance[7] = min(left, up);
	}

	Square distance[8];
	
private:
	Square left;
	Square right;
	Square up;
	Square down;

	Square DistanceToBorderUp(Square yPos) {
		return NORTH - yPos - 1;
	}

	Square DistanceToBorderDown(Square yPos) {
		return yPos;
	}

	Square DistanceToBorderLeft(Square xPos) {
		return xPos;
	}

	Square DistanceToBorderRight(Square xPos) {
		return NORTH - xPos - 1;
	}
};

class KnightDistance {

public:
	KnightDistance(Square pos) {
		Square xPos = getCol(pos);
		Square yPos = getRow(pos);
		Square left = DistanceToBorderLeft(xPos);
		Square right = DistanceToBorderRight(xPos);
		Square up = DistanceToBorderUp(yPos);
		Square down = DistanceToBorderDown(yPos);
		
		distance[0] = min(left, up - 1);
		distance[1] = min(left, down - 1);
		distance[2] = min(right, up - 1);
		distance[3] = min(right, down - 1);
		distance[4] = min(left - 1, up);
		distance[5] = min(right - 1, up);
		distance[6] = min(left - 1, down);
		distance[7] = min(right - 1, down);

	}

	Square distance[8];

private:

	Square DistanceToBorderUp(Square yPos) {
		return NORTH - yPos - 1;
	}

	Square DistanceToBorderDown(Square yPos) {
		return yPos;
	}

	Square DistanceToBorderLeft(Square xPos) {
		return xPos;
	}

	Square DistanceToBorderRight(Square xPos) {
		return NORTH - xPos - 1;
	}
};


class AttackTest {
public:

};

#endif