// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/TetrisTypes.h"

/**
 * SRS 표준 피스 데이터 (constexpr 테이블).
 *
 * Why constexpr 우선: Phase 1에서는 단위 테스트 가능성과 결정성이 우선.
 *   DataAsset 래핑은 Phase 2(에디터 노출 시)에서 동일 인터페이스로 교체 가능.
 * 좌표계: 피봇 기준 상대 좌표. (X 오른쪽, Y 위쪽).
 *
 * 회전 표현:
 *   각 피스는 4개의 (회전 상태) 슬롯을 가지며, 각 슬롯은 4개의 블록 좌표를 갖는다.
 *   StateR = CW 1회, State2 = 180°, StateL = CCW 1회.
 */
namespace TetrisPieceData
{
	/** 피스 1종의 4방향 × 4블록 좌표 묶음. */
	struct FPieceShape
	{
		FIntPoint Blocks[4][4]; // [RotationState][BlockIndex]
		int32 SpawnOffsetX;     // 스폰 시 피봇 X 좌표
	};

	// I 피스 — 4x1 (또는 1x4). SRS 표준에 맞춰 4상태 모두 정의.
	// 0:  XXXX           R: . X .          2: . . . .          L: . X .
	//                       . X .             X X X X             . X .
	//                       . X .             . . . .             . X .
	//                       . X .                                  . X .
	inline const FPieceShape Shape_I = {
		{
			// State0
			{ FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 1,  0), FIntPoint( 2,  0) },
			// StateR
			{ FIntPoint( 1,  1), FIntPoint( 1,  0), FIntPoint( 1, -1), FIntPoint( 1, -2) },
			// State2
			{ FIntPoint(-1, -1), FIntPoint( 0, -1), FIntPoint( 1, -1), FIntPoint( 2, -1) },
			// StateL
			{ FIntPoint( 0,  1), FIntPoint( 0,  0), FIntPoint( 0, -1), FIntPoint( 0, -2) },
		},
		4
	};

	// O 피스 — 회전 불변, 4상태 모두 동일 좌표.
	inline const FPieceShape Shape_O = {
		{
			{ FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(1, 1) },
			{ FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(1, 1) },
			{ FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(1, 1) },
			{ FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(1, 1) },
		},
		4
	};

	// T 피스
	inline const FPieceShape Shape_T = {
		{
			// State0:  . X .
			//          X X X
			{ FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 1,  0), FIntPoint( 0,  1) },
			// StateR:  . X .
			//          . X X
			//          . X .
			{ FIntPoint( 0,  1), FIntPoint( 0,  0), FIntPoint( 1,  0), FIntPoint( 0, -1) },
			// State2:  X X X
			//          . X .
			{ FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 1,  0), FIntPoint( 0, -1) },
			// StateL:  . X .
			//          X X .
			//          . X .
			{ FIntPoint( 0,  1), FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 0, -1) },
		},
		4
	};

	// S 피스
	inline const FPieceShape Shape_S = {
		{
			// State0:  . X X
			//          X X .
			{ FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 0,  1), FIntPoint( 1,  1) },
			// StateR:  . X .
			//          . X X
			//          . . X
			{ FIntPoint( 0,  1), FIntPoint( 0,  0), FIntPoint( 1,  0), FIntPoint( 1, -1) },
			// State2:  . X X
			//          X X .
			{ FIntPoint(-1, -1), FIntPoint( 0, -1), FIntPoint( 0,  0), FIntPoint( 1,  0) },
			// StateL:  X . .
			//          X X .
			//          . X .
			{ FIntPoint(-1,  1), FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 0, -1) },
		},
		4
	};

	// Z 피스
	inline const FPieceShape Shape_Z = {
		{
			// State0:  X X .
			//          . X X
			{ FIntPoint(-1,  1), FIntPoint( 0,  1), FIntPoint( 0,  0), FIntPoint( 1,  0) },
			// StateR:  . . X
			//          . X X
			//          . X .
			{ FIntPoint( 1,  1), FIntPoint( 0,  0), FIntPoint( 1,  0), FIntPoint( 0, -1) },
			// State2: X X .
			//         . X X
			{ FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 0, -1), FIntPoint( 1, -1) },
			// StateL:  . X .
			//          X X .
			//          X . .
			{ FIntPoint( 0,  1), FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint(-1, -1) },
		},
		4
	};

	// J 피스
	inline const FPieceShape Shape_J = {
		{
			// State0:  X . .
			//          X X X
			{ FIntPoint(-1,  1), FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 1,  0) },
			// StateR:  . X X
			//          . X .
			//          . X .
			{ FIntPoint( 0,  1), FIntPoint( 1,  1), FIntPoint( 0,  0), FIntPoint( 0, -1) },
			// State2:  X X X
			//          . . X
			{ FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 1,  0), FIntPoint( 1, -1) },
			// StateL:  . X .
			//          . X .
			//          X X .
			{ FIntPoint( 0,  1), FIntPoint( 0,  0), FIntPoint(-1, -1), FIntPoint( 0, -1) },
		},
		4
	};

	// L 피스
	inline const FPieceShape Shape_L = {
		{
			// State0:  . . X
			//          X X X
			{ FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 1,  0), FIntPoint( 1,  1) },
			// StateR:  . X .
			//          . X .
			//          . X X
			{ FIntPoint( 0,  1), FIntPoint( 0,  0), FIntPoint( 0, -1), FIntPoint( 1, -1) },
			// State2:  X X X
			//          X . .
			{ FIntPoint(-1,  0), FIntPoint( 0,  0), FIntPoint( 1,  0), FIntPoint(-1, -1) },
			// StateL:  X X .
			//          . X .
			//          . X .
			{ FIntPoint(-1,  1), FIntPoint( 0,  1), FIntPoint( 0,  0), FIntPoint( 0, -1) },
		},
		4
	};

	/** PieceType → Shape 매핑. None/Garbage는 nullptr. */
	inline const FPieceShape* GetShape(EPieceType Type)
	{
		switch (Type)
		{
			case EPieceType::I: return &Shape_I;
			case EPieceType::O: return &Shape_O;
			case EPieceType::T: return &Shape_T;
			case EPieceType::S: return &Shape_S;
			case EPieceType::Z: return &Shape_Z;
			case EPieceType::J: return &Shape_J;
			case EPieceType::L: return &Shape_L;
			default:            return nullptr;
		}
	}

	/**
	 * SRS Wall Kick 테이블.
	 *
	 * 인덱스 규칙:
	 *   - 8개 전이: 0→R, R→0, R→2, 2→R, 2→L, L→2, L→0, 0→L
	 *   - 각 전이별 5개 Test 오프셋 (Test1은 (0,0) — 회전 그 자체).
	 *
	 * SRS 표준 (TetrisWiki 기준):
	 *   JLSTZ는 한 그룹, I는 별도. O는 Kick 없음.
	 *
	 * 좌표는 (X, Y) — Y는 위 방향 양수 (TetrisBoard와 동일).
	 */
	enum class EKickGroup : uint8
	{
		JLSTZ,
		I,
		None, // O 피스
	};

	/** 회전 전이 인덱스: From*4 + To 형태가 아닌, 명시적 8가지. */
	inline int32 KickTransitionIndex(ERotationState From, ERotationState To)
	{
		// 0=0→R, 1=R→0, 2=R→2, 3=2→R, 4=2→L, 5=L→2, 6=L→0, 7=0→L
		const uint8 F = static_cast<uint8>(From);
		const uint8 T = static_cast<uint8>(To);
		if (F == 0 && T == 1) return 0;
		if (F == 1 && T == 0) return 1;
		if (F == 1 && T == 2) return 2;
		if (F == 2 && T == 1) return 3;
		if (F == 2 && T == 3) return 4;
		if (F == 3 && T == 2) return 5;
		if (F == 3 && T == 0) return 6;
		if (F == 0 && T == 3) return 7;
		return INDEX_NONE;
	}

	// JLSTZ Kick Table (5 tests per transition, Test1은 항상 (0,0))
	// 출처: SRS 표준
	inline const FIntPoint KickTable_JLSTZ[8][5] = {
		// 0→R
		{ FIntPoint( 0, 0), FIntPoint(-1, 0), FIntPoint(-1,+1), FIntPoint( 0,-2), FIntPoint(-1,-2) },
		// R→0
		{ FIntPoint( 0, 0), FIntPoint(+1, 0), FIntPoint(+1,-1), FIntPoint( 0,+2), FIntPoint(+1,+2) },
		// R→2
		{ FIntPoint( 0, 0), FIntPoint(+1, 0), FIntPoint(+1,-1), FIntPoint( 0,+2), FIntPoint(+1,+2) },
		// 2→R
		{ FIntPoint( 0, 0), FIntPoint(-1, 0), FIntPoint(-1,+1), FIntPoint( 0,-2), FIntPoint(-1,-2) },
		// 2→L
		{ FIntPoint( 0, 0), FIntPoint(+1, 0), FIntPoint(+1,+1), FIntPoint( 0,-2), FIntPoint(+1,-2) },
		// L→2
		{ FIntPoint( 0, 0), FIntPoint(-1, 0), FIntPoint(-1,-1), FIntPoint( 0,+2), FIntPoint(-1,+2) },
		// L→0
		{ FIntPoint( 0, 0), FIntPoint(-1, 0), FIntPoint(-1,-1), FIntPoint( 0,+2), FIntPoint(-1,+2) },
		// 0→L
		{ FIntPoint( 0, 0), FIntPoint(+1, 0), FIntPoint(+1,+1), FIntPoint( 0,-2), FIntPoint(+1,-2) },
	};

	// I-Piece Kick Table
	inline const FIntPoint KickTable_I[8][5] = {
		// 0→R
		{ FIntPoint( 0, 0), FIntPoint(-2, 0), FIntPoint(+1, 0), FIntPoint(-2,-1), FIntPoint(+1,+2) },
		// R→0
		{ FIntPoint( 0, 0), FIntPoint(+2, 0), FIntPoint(-1, 0), FIntPoint(+2,+1), FIntPoint(-1,-2) },
		// R→2
		{ FIntPoint( 0, 0), FIntPoint(-1, 0), FIntPoint(+2, 0), FIntPoint(-1,+2), FIntPoint(+2,-1) },
		// 2→R
		{ FIntPoint( 0, 0), FIntPoint(+1, 0), FIntPoint(-2, 0), FIntPoint(+1,-2), FIntPoint(-2,+1) },
		// 2→L
		{ FIntPoint( 0, 0), FIntPoint(+2, 0), FIntPoint(-1, 0), FIntPoint(+2,+1), FIntPoint(-1,-2) },
		// L→2
		{ FIntPoint( 0, 0), FIntPoint(-2, 0), FIntPoint(+1, 0), FIntPoint(-2,-1), FIntPoint(+1,+2) },
		// L→0
		{ FIntPoint( 0, 0), FIntPoint(+1, 0), FIntPoint(-2, 0), FIntPoint(+1,-2), FIntPoint(-2,+1) },
		// 0→L
		{ FIntPoint( 0, 0), FIntPoint(-1, 0), FIntPoint(+2, 0), FIntPoint(-1,+2), FIntPoint(+2,-1) },
	};

	inline EKickGroup GetKickGroup(EPieceType Type)
	{
		switch (Type)
		{
			case EPieceType::I: return EKickGroup::I;
			case EPieceType::O: return EKickGroup::None;
			case EPieceType::J:
			case EPieceType::L:
			case EPieceType::S:
			case EPieceType::T:
			case EPieceType::Z:
				return EKickGroup::JLSTZ;
			default:
				return EKickGroup::None;
		}
	}
}
