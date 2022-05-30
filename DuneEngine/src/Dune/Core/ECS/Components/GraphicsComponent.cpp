#include "pch.h"
#include "GraphicsComponent.h"

namespace Dune
{
	const dVector<dU32> GraphicsComponent::m_defaultCubeIndices =
	{
		0, 1, 2, 0, 2, 3,			//Face
		4, 6, 5, 4, 7, 6,			//Back
		10, 11, 9, 10, 9, 8,		//Left
		13, 12, 14, 13, 14, 15,		//Right
		16, 18, 19, 16, 19, 17,		//Top
		22, 20, 21, 22, 21, 23		//Botoom
	};

	const dVector<Vertex> GraphicsComponent::m_defaultCubeVertices =
	{
		{ {-0.5f, -0.5f, -0.5f},	{0.0f, 0.0f, 0.0f, 1.0f},	{ 0.0f, 0.0f, -1.0f } }, // 0
		{ {-0.5f,  0.5f, -0.5f},	{0.0f, 1.0f, 0.0f, 1.0f},	{ 0.0f, 0.0f, -1.0f } }, // 1
		{ { 0.5f,  0.5f, -0.5f},	{1.0f, 1.0f, 0.0f, 1.0f},	{ 0.0f, 0.0f, -1.0f } }, // 2
		{ { 0.5f, -0.5f, -0.5f},	{1.0f, 0.0f, 0.0f, 1.0f},	{ 0.0f, 0.0f, -1.0f } }, // 3

		{ {-0.5f, -0.5f,  0.5f},	{0.0f, 0.0f, 1.0f, 1.0f},	{ 0.0f, 0.0f,  1.0f } }, // 4
		{ {-0.5f,  0.5f,  0.5f},	{0.0f, 1.0f, 1.0f, 1.0f},	{ 0.0f, 0.0f,  1.0f } }, // 5
		{ { 0.5f,  0.5f,  0.5f},	{1.0f, 1.0f, 1.0f, 1.0f},	{ 0.0f, 0.0f,  1.0f } }, // 6
		{ { 0.5f, -0.5f,  0.5f},	{1.0f, 0.0f, 1.0f, 1.0f},	{ 0.0f, 0.0f,  1.0f } }, // 7

		{ {-0.5f, -0.5f, -0.5f},	{0.0f, 0.0f, 0.0f, 1.0f},	{-1.0f, 0.0f,  0.0f } }, // 8
		{ {-0.5f,  0.5f, -0.5f},	{0.0f, 1.0f, 0.0f, 1.0f},	{-1.0f, 0.0f,  0.0f } }, // 9
		{ {-0.5f, -0.5f,  0.5f},	{0.0f, 0.0f, 1.0f, 1.0f},	{-1.0f, 0.0f,  0.0f } }, // 10
		{ {-0.5f,  0.5f,  0.5f},	{0.0f, 1.0f, 1.0f, 1.0f},	{-1.0f, 0.0f,  0.0f } }, // 11

		{ { 0.5f,  0.5f, -0.5f},	{1.0f, 1.0f, 0.0f, 1.0f},	{ 1.0f, 0.0f,  0.0f } }, // 12
		{ { 0.5f, -0.5f, -0.5f},	{1.0f, 0.0f, 0.0f, 1.0f},	{ 1.0f, 0.0f,  0.0f } }, // 13
		{ { 0.5f,  0.5f,  0.5f},	{1.0f, 1.0f, 1.0f, 1.0f},	{ 1.0f, 0.0f,  0.0f } }, // 14
		{ { 0.5f, -0.5f,  0.5f},	{1.0f, 0.0f, 1.0f, 1.0f},	{ 1.0f, 0.0f,  0.0f } }, // 15

		{ {-0.5f,  0.5f, -0.5f},	{0.0f, 1.0f, 0.0f, 1.0f},	{ 0.0f, 1.0f, 0.0f } }, // 16
		{ { 0.5f,  0.5f, -0.5f},	{1.0f, 1.0f, 0.0f, 1.0f},	{ 0.0f, 1.0f, 0.0f } }, // 17
		{ {-0.5f,  0.5f,  0.5f},	{0.0f, 1.0f, 1.0f, 1.0f},	{ 0.0f, 1.0f, 0.0f } }, // 18
		{ { 0.5f,  0.5f,  0.5f},	{1.0f, 1.0f, 1.0f, 1.0f},	{ 0.0f, 1.0f, 0.0f } }, // 19

		{ {-0.5f, -0.5f, -0.5f},	{0.0f, 0.0f, 0.0f, 1.0f},	{ 0.0f, -1.0f, 0.0f } }, // 20
		{ { 0.5f, -0.5f, -0.5f},	{1.0f, 0.0f, 0.0f, 1.0f},	{ 0.0f, -1.0f, 0.0f } }, // 21
		{ {-0.5f, -0.5f,  0.5f},	{0.0f, 0.0f, 1.0f, 1.0f},	{ 0.0f, -1.0f, 0.0f } }, // 22
		{ { 0.5f, -0.5f,  0.5f},	{1.0f, 0.0f, 1.0f, 1.0f},	{ 0.0f, -1.0f, 0.0f } }, // 23
	};

	const std::shared_ptr<Mesh> GraphicsComponent::m_defaultMesh = std::make_shared<Mesh>(Mesh(GraphicsComponent::m_defaultCubeIndices, GraphicsComponent::m_defaultCubeVertices));
}