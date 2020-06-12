#include "source/GcLib/pch.h"

#include "StgCommon.hpp"
#include "StgSystem.hpp"

/**********************************************************
//StgMoveObject
**********************************************************/
StgMoveObject::StgMoveObject(StgStageController* stageController) {
	posX_ = 0;
	posY_ = 0;
	framePattern_ = 0;
	stageController_ = stageController;

	pattern_ = nullptr;
	bEnableMovement_ = true;
}
StgMoveObject::~StgMoveObject() {
	pattern_ = nullptr;
}
void StgMoveObject::_Move() {
	if (pattern_ == nullptr || !bEnableMovement_) return;

	if (mapPattern_.size() > 0) {
		auto itr = mapPattern_.begin();
		int frame = itr->first;
		if (frame == framePattern_) {
			std::list<shared_ptr<StgMovePattern>>& patternList = itr->second;
			while (patternList.size() > 0) {
				_AttachReservedPattern(patternList.front());
				patternList.pop_front();
			}
			mapPattern_.erase(itr);
		}
	}

	pattern_->Move();
	++framePattern_;
}
void StgMoveObject::_AttachReservedPattern(std::shared_ptr<StgMovePattern> pattern) {
	if (pattern_ == nullptr)
		pattern_ = std::shared_ptr<StgMovePattern_Angle>(new StgMovePattern_Angle(this));

	/*
	int newMoveType = pattern->GetType();
	if (newMoveType == StgMovePattern::TYPE_ANGLE) {
		StgMovePattern_Angle* angPattern = dynamic_cast<StgMovePattern_Angle*>(pattern.get());
		if (angPattern->GetSpeed() == StgMovePattern::NO_CHANGE)
			angPattern->SetSpeed(pattern_->GetSpeed());
		if (angPattern->GetDirectionAngle() == StgMovePattern::NO_CHANGE)
			angPattern->SetDirectionAngle(pattern_->GetDirectionAngle());

		if (StgMovePattern_Angle* patternSrcAng = (StgMovePattern_Angle*)pattern_.get()) {
			if (angPattern->acceleration_ == StgMovePattern::NO_CHANGE)
				angPattern->SetAcceleration(patternSrcAng->acceleration_);
			if (angPattern->maxSpeed_ == StgMovePattern::NO_CHANGE)
				angPattern->SetMaxSpeed(patternSrcAng->maxSpeed_);
			if (angPattern->angularVelocity_ == StgMovePattern::NO_CHANGE)
				angPattern->SetAngularVelocity(patternSrcAng->angularVelocity_);
		}
	}
	else if (newMoveType == StgMovePattern::TYPE_XY) {
		StgMovePattern_XY* xyPattern = dynamic_cast<StgMovePattern_XY*>(pattern.get());

		double speed = pattern_->GetSpeed();
		//double angle = pattern_->GetDirectionAngle();
		double speedX = speed * pattern_->c_;
		double speedY = speed * pattern_->s_;

		if (xyPattern->GetSpeedX() == StgMovePattern::NO_CHANGE)
			xyPattern->SetSpeedX(speedX);
		if (xyPattern->GetSpeedY() == StgMovePattern::NO_CHANGE)
			xyPattern->SetSpeedY(speedY);
	}
	*/

	pattern->_Activate(pattern_.get());
	pattern_ = pattern;
}
void StgMoveObject::AddPattern(int frameDelay, std::shared_ptr<StgMovePattern> pattern, bool bForceMap) {
	if (frameDelay == 0 && !bForceMap)
		_AttachReservedPattern(pattern);
	else {
		int frame = frameDelay + framePattern_;
		mapPattern_[frame].push_back(pattern);
	}
}
double StgMoveObject::GetSpeed() {
	if (pattern_ == nullptr) return 0;
	double res = pattern_->GetSpeed();
	return res;
}
void StgMoveObject::SetSpeed(double speed) {
	if (pattern_ == nullptr || pattern_->GetType() != StgMovePattern::TYPE_ANGLE) {
		pattern_ = std::shared_ptr<StgMovePattern_Angle>(new StgMovePattern_Angle(this));
	}
	StgMovePattern_Angle* pattern = dynamic_cast<StgMovePattern_Angle*>(pattern_.get());
	pattern->SetSpeed(speed);
}
double StgMoveObject::GetDirectionAngle() {
	if (pattern_ == nullptr)return 0;
	double res = pattern_->GetDirectionAngle();
	return res;
}
void StgMoveObject::SetDirectionAngle(double angle) {
	if (pattern_ == nullptr || pattern_->GetType() != StgMovePattern::TYPE_ANGLE) {
		pattern_ = std::shared_ptr<StgMovePattern_Angle>(new StgMovePattern_Angle(this));
	}
	StgMovePattern_Angle* pattern = dynamic_cast<StgMovePattern_Angle*>(pattern_.get());
	pattern->SetDirectionAngle(angle);
}
void StgMoveObject::SetSpeedX(double speedX) {
	if (pattern_ == nullptr || pattern_->GetType() != StgMovePattern::TYPE_XY) {
		pattern_ = std::shared_ptr<StgMovePattern_XY>(new StgMovePattern_XY(this));
	}
	StgMovePattern_XY* pattern = dynamic_cast<StgMovePattern_XY*>(pattern_.get());
	pattern->SetSpeedX(speedX);
}
void StgMoveObject::SetSpeedY(double speedY) {
	if (pattern_ == nullptr || pattern_->GetType() != StgMovePattern::TYPE_XY) {
		pattern_ = std::shared_ptr<StgMovePattern_XY>(new StgMovePattern_XY(this));
	}
	StgMovePattern_XY* pattern = dynamic_cast<StgMovePattern_XY*>(pattern_.get());
	pattern->SetSpeedY(speedY);
}

/**********************************************************
//StgMovePattern
**********************************************************/
//StgMovePattern
StgMovePattern::StgMovePattern(StgMoveObject* target) {
	target_ = target;
	idShotData_ = NO_CHANGE;
	frameWork_ = 0;
	typeMove_ = TYPE_OTHER;
	c_ = 1;
	s_ = 0;
}
shared_ptr<StgMoveObject> StgMovePattern::_GetMoveObject(int id) {
	shared_ptr<DxScriptObjectBase> base = _GetStageController()->GetMainRenderObject(id);
	if (base == nullptr || base->IsDeleted())return nullptr;

	return std::dynamic_pointer_cast<StgMoveObject>(base);
}

//StgMovePattern_Angle
StgMovePattern_Angle::StgMovePattern_Angle(StgMoveObject* target) : StgMovePattern(target) {
	typeMove_ = TYPE_ANGLE;
	speed_ = 0;
	angDirection_ = 0;
	acceleration_ = 0;
	maxSpeed_ = 0;
	angularVelocity_ = 0;
	idRelative_ = weak_ptr<StgMoveObject>();
}
void StgMovePattern_Angle::Move() {
	double angle = angDirection_;

	if (acceleration_ != 0) {
		speed_ += acceleration_;
		if (acceleration_ > 0)
			speed_ = std::min(speed_, maxSpeed_);
		if (acceleration_ < 0)
			speed_ = std::max(speed_, maxSpeed_);
	}
	if (angularVelocity_ != 0) {
		SetDirectionAngle(angle + angularVelocity_);
	}

	double sx = speed_ * c_;
	double sy = speed_ * s_;
	double px = target_->GetPositionX() + sx;
	double py = target_->GetPositionY() + sy;

	target_->SetPositionX(px);
	target_->SetPositionY(py);

	++frameWork_;
}
void StgMovePattern_Angle::_Activate(StgMovePattern* _src) {
	double newSpeed = 0;
	double newAngle = 0;
	double newAccel = 0;
	double newAgVel = 0;
	double newMaxSp = 0;
	if (_src->GetType() == TYPE_ANGLE) {
		StgMovePattern_Angle* src = (StgMovePattern_Angle*)_src;
		newSpeed = src->speed_;
		newAngle = src->angDirection_;
		newAccel = src->acceleration_;
		newAgVel = src->angularVelocity_;
		newMaxSp = src->maxSpeed_;
	}
	else if (_src->GetType() == TYPE_XY) {
		StgMovePattern_XY* src = (StgMovePattern_XY*)_src;
		newSpeed = src->GetSpeed();
		newAngle = src->GetDirectionAngle();
		newAccel = hypot(src->GetAccelerationX(), src->GetAccelerationY());
		newMaxSp = hypot(src->GetMaxSpeedX(), src->GetMaxSpeedY());
	}

	for (auto& pairCmd : listCommand_) {
		double arg = pairCmd.second;
		switch (pairCmd.first) {
		case SET_ZERO:
			newAccel = 0;
			newAgVel = 0;
			newMaxSp = 0;
			break;
		case SET_SPEED:
			newSpeed = arg;
			break;
		case SET_ANGLE:
			newAngle = arg;
			break;
		case SET_ACCEL:
			newAccel = arg;
			break;
		case SET_AGVEL:
			newAgVel = arg;
			break;
		case SET_SPMAX:
			newMaxSp = arg;
			break;
		case ADD_SPEED:
			newSpeed += arg;
			break;
		case ADD_ANGLE:
			newAngle += arg;
			break;
		case ADD_ACCEL:
			newAccel += arg;
			break;
		case ADD_AGVEL:
			newAgVel += arg;
			break;
		case ADD_SPMAX:
			newMaxSp += arg;
			break;
		}
	}

	if (shared_ptr<StgMoveObject> obj = idRelative_.lock()) {
		double px = target_->GetPositionX();
		double py = target_->GetPositionY();
		double tx = obj->GetPositionX();
		double ty = obj->GetPositionY();
		newAngle += atan2(ty - py, tx - px);
	}
	speed_ = newSpeed;
	//angDirection_ = newAngle;
	SetDirectionAngle(newAngle);
	acceleration_ = newAccel;
	angularVelocity_ = newAgVel;
	maxSpeed_ = newMaxSp;
}
void StgMovePattern_Angle::SetDirectionAngle(double angle) {
	if (angle != StgMovePattern::NO_CHANGE) {
		angle = Math::NormalizeAngleRad(angle);
		c_ = cos(angle);
		s_ = sin(angle);
	}
	angDirection_ = angle;
}

//StgMovePattern_XY
StgMovePattern_XY::StgMovePattern_XY(StgMoveObject* target) : StgMovePattern(target) {
	typeMove_ = TYPE_XY;
	c_ = 0;
	s_ = 0;
	accelerationX_ = 0;
	accelerationY_ = 0;
	maxSpeedX_ = INT_MAX;
	maxSpeedY_ = INT_MAX;
}
void StgMovePattern_XY::Move() {
	if (accelerationX_ != 0) {
		c_ += accelerationX_;
		if (accelerationX_ > 0)
			c_ = std::min(c_, maxSpeedX_);
		if (accelerationX_ < 0)
			c_ = std::max(c_, maxSpeedX_);
	}
	if (accelerationY_ != 0) {
		s_ += accelerationY_;
		if (accelerationY_ > 0)
			s_ = std::min(s_, maxSpeedY_);
		if (accelerationY_ < 0)
			s_ = std::max(s_, maxSpeedY_);
	}

	double px = target_->GetPositionX() + c_;
	double py = target_->GetPositionY() + s_;

	target_->SetPositionX(px);
	target_->SetPositionY(py);

	++frameWork_;
}
void StgMovePattern_XY::_Activate(StgMovePattern* _src) {
	if (_src->GetType() == TYPE_XY) {
		StgMovePattern_XY* src = (StgMovePattern_XY*)_src;
		c_ = src->c_;
		s_ = src->s_;
		accelerationX_ = src->accelerationX_;
		accelerationY_ = src->accelerationY_;
		maxSpeedX_ = src->maxSpeedX_;
		maxSpeedY_ = src->maxSpeedY_;
	}
	else if (_src->GetType() == TYPE_ANGLE) {
		StgMovePattern_Angle* src = (StgMovePattern_Angle*)_src;
		c_ = _src->GetSpeedX();
		s_ = _src->GetSpeedY();
		accelerationX_ = src->acceleration_ * c_;
		accelerationY_ = src->acceleration_ * s_;
		maxSpeedX_ = src->maxSpeed_ * c_;
		maxSpeedY_ = src->maxSpeed_ * s_;
	}

	for (auto& pairCmd : listCommand_) {
		double arg = pairCmd.second;
		switch (pairCmd.first) {
		case SET_ZERO:
			accelerationX_ = 0;
			accelerationY_ = 0;
			maxSpeedX_ = 0;
			maxSpeedY_ = 0;
			break;
		case SET_S_X:
			c_ = arg;
			break;
		case SET_S_Y:
			s_ = arg;
			break;
		case SET_A_X:
			accelerationX_ = arg;
			break;
		case SET_A_Y:
			accelerationY_ = arg;
			break;
		case SET_M_X:
			maxSpeedX_ = arg;
			break;
		case SET_M_Y:
			maxSpeedY_ = arg;
			break;
		}
	}
}

//StgMovePattern_Line
StgMovePattern_Line::StgMovePattern_Line(StgMoveObject* target) : StgMovePattern(target) {
	typeMove_ = TYPE_NONE;
	speed_ = 0;
	angDirection_ = 0;
	weight_ = 0;
	maxSpeed_ = 0;
	frameStop_ = 0;
	c_ = 0;
	s_ = 0;
}
void StgMovePattern_Line::Move() {
	if (typeLine_ == TYPE_SPEED || typeLine_ == TYPE_FRAME) {
		double sx = speed_ * c_;
		double sy = speed_ * s_;
		double px = target_->GetPositionX() + sx;
		double py = target_->GetPositionY() + sy;

		target_->SetPositionX(px);
		target_->SetPositionY(py);
		frameStop_--;
		if (frameStop_ <= 0) {
			typeLine_ = TYPE_NONE;
			speed_ = 0;
		}
	}
	else if (typeLine_ == TYPE_WEIGHT) {
		double nx = target_->GetPositionX();
		double ny = target_->GetPositionY();
		if (dist_ < 1) {
			typeLine_ = TYPE_NONE;
			speed_ = 0;
		}
		else {
			speed_ = dist_ / weight_;
			if (speed_ > maxSpeed_)
				speed_ = maxSpeed_;
			double px = target_->GetPositionX() + speed_ * c_;
			double py = target_->GetPositionY() + speed_ * s_;
			target_->SetPositionX(px);
			target_->SetPositionY(py);

			dist_ -= speed_;
		}
	}
}
void StgMovePattern_Line::SetAtSpeed(double tx, double ty, double speed) {
	typeLine_ = TYPE_SPEED;
	toX_ = tx;
	toY_ = ty;
	double nx = tx - target_->GetPositionX();
	double ny = ty - target_->GetPositionY();
	dist_ = sqrt(nx * nx + ny * ny);
	speed_ = speed;
	angDirection_ = atan2(ny, nx);
	frameStop_ = dist_ / speed;

	c_ = cos(angDirection_);
	s_ = sin(angDirection_);
}
void StgMovePattern_Line::SetAtFrame(double tx, double ty, double frame) {
	typeLine_ = TYPE_FRAME;
	toX_ = tx;
	toY_ = ty;
	double nx = tx - target_->GetPositionX();
	double ny = ty - target_->GetPositionY();
	dist_ = sqrt(nx * nx + ny * ny);
	speed_ = dist_ / frame;
	angDirection_ = atan2(ny, nx);
	frameStop_ = frame;

	c_ = cos(angDirection_);
	s_ = sin(angDirection_);
}
void StgMovePattern_Line::SetAtWait(double tx, double ty, double weight, double maxSpeed) {
	typeLine_ = TYPE_WEIGHT;
	toX_ = tx;
	toY_ = ty;
	weight_ = weight;
	maxSpeed_ = maxSpeed;
	double nx = tx - target_->GetPositionX();
	double ny = ty - target_->GetPositionY();
	dist_ = sqrt(nx * nx + ny * ny);
	speed_ = maxSpeed_;
	angDirection_ = atan2(ny, nx);

	c_ = cos(angDirection_);
	s_ = sin(angDirection_);
}
