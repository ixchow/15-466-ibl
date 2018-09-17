#include "Game.hpp"

void Game::update(float time) {
	ball += ball_velocity * time;
	if (ball.x >= 0.5f * FrameWidth - BallRadius) {
		ball_velocity.x = -std::abs(ball_velocity.x);
	}
	if (ball.x <=-0.5f * FrameWidth + BallRadius) {
		ball_velocity.x = std::abs(ball_velocity.x);
	}
	if (ball.y >= 0.5f * FrameHeight - BallRadius) {
		ball_velocity.y = -std::abs(ball_velocity.y);
	}
	if (ball.y <=-0.5f * FrameHeight + BallRadius) {
		ball_velocity.y = std::abs(ball_velocity.y);
	}

	auto do_point = [this](glm::vec2 const &pt) {
		glm::vec2 to = ball - pt;
		float len2 = glm::dot(to, to);
		if (len2 > BallRadius * BallRadius) return;
		//if point is inside ball, make ball velocity outward-going:
		float d = glm::dot(ball_velocity, to);
		ball_velocity += ((std::abs(d) - d) / len2) * to;
	};

	do_point(glm::vec2(paddle.x - 0.5f * PaddleWidth, paddle.y));
	do_point(glm::vec2(paddle.x + 0.5f * PaddleWidth, paddle.y));

	auto do_edge = [&](glm::vec2 const &a, glm::vec2 const &b) {
		float along = glm::dot(ball-a, b-a);
		float max = glm::dot(b-a,b-a);
		if (along <= 0.0f || along >= max) return;
		do_point(glm::mix(a,b,along/max));
	};

	do_edge(glm::vec2(paddle.x + 0.5f * PaddleWidth, paddle.y), glm::vec2(paddle.x - 0.5f * PaddleWidth, paddle.y));

}
