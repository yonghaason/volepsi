#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <stack>
#include <fstream>
#include <numeric>
#include <random>
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Common/BitVector.h>
#include "benes.h"

using namespace std;
using namespace oc;

void Benes::initialize(int N, vector<int> perm)
{
	mN = N;
	mLogN = int(ceil(log2(N)));
	mNumColumns = 2 * mLogN - 1;

	if (perm.size())
	{
		assert(perm.size() == N);
		mPerm = perm;
	}
	else
	{
		mPerm.resize(N);
		std::iota(mPerm.begin(), mPerm.end(), 0);

		std::random_device rd;
		std::shuffle(mPerm.begin(), mPerm.end(), std::default_random_engine(rd()));
	}

	mInvPerm.resize(N);
	for (size_t i = 0; i < N; i++)
	{
		mInvPerm[mPerm[i]] = i;
	}

	permInner.resize(N);
	invPermInner.resize(N);

	mSwitches.resize(mNumColumns);
	for (int i = 0; i < mNumColumns; ++i)
		mSwitches[i].resize(mN / 2);
}

// void Benes::DFS(int idx, int route,
// 	vector<char>& path, vector<int> &perm, vector<int> &invPerm)
void Benes::DFS(int idx, int route,
								vector<char> &path)
{
	stack<pair<int, int>> st;
	st.push({idx, route});
	pair<int, int> pr;
	while (!st.empty())
	{
		pr = st.top();
		st.pop();
		path[pr.first] = pr.second;
		if (path[pr.first ^ 1] < 0) // if the next item in the vertical array is unassigned
			st.push({pr.first ^ 1,
							 pr.second ^ 1}); /// the next item is always assigned the opposite of this item,
																/// unless it was part of path/cycle of previous node

		idx = permInner[invPermInner[pr.first] ^ 1];
		if (path[idx] < 0)
			st.push({idx, pr.second ^ 1});
	}
}

int shuffle(int i, int n)
{
	return ((i & 1) << (n - 1)) | (i >> 1);
}

void Benes::genBenesRouteInner(int depth, int permIdx, const vector<int> &src,
															 const vector<int> &dest)
{
	int i, j, x, s;
	vector<int> bottom1;
	vector<int> top1;
	int subNetSize = src.size();

	int coDepth = mLogN - depth;

	/*
	 * daca avem doar un nivel in retea
	 */
	if (subNetSize == 2)
	{
		if (coDepth == 1)
			mSwitches[depth][permIdx] = src[0] != dest[0];
		else
			mSwitches[depth + 1][permIdx] = src[0] != dest[0];
		return;
	}

	if (subNetSize == 3)
	{
		if (src[0] == dest[0])
		{
			mSwitches[depth][permIdx] = 0;
			mSwitches[depth + 2][permIdx] = 0;
			if (src[1] == dest[1])
				mSwitches[depth + 1][permIdx] = 0;
			else
				mSwitches[depth + 1][permIdx] = 1;
		}

		if (src[0] == dest[1])
		{
			mSwitches[depth][permIdx] = 0;
			mSwitches[depth + 2][permIdx] = 1;
			if (src[1] == dest[0])
				mSwitches[depth + 1][permIdx] = 0;
			else
				mSwitches[depth + 1][permIdx] = 1;
		}

		if (src[0] == dest[2])
		{
			mSwitches[depth][permIdx] = 1;
			mSwitches[depth + 1][permIdx] = 1;
			if (src[1] == dest[0])
				mSwitches[depth + 2][permIdx] = 0;
			else
				mSwitches[depth + 2][permIdx] = 1;
		}
		return;
	}

	/*
	 * aflam dimensiunea retelei benes
	 */
	int levels = 2 * coDepth - 1;

	vector<int> bottom2(subNetSize / 2);
	vector<int> top2(int(ceil(subNetSize * 0.5)));

	/*
	 * destinatia este o permutare a intrari
	 */

	for (i = 0; i < subNetSize; ++i)
		invPermInner[src[i]] = i;

	for (i = 0; i < subNetSize; ++i)
		permInner[i] = invPermInner[dest[i]];

	for (i = 0; i < subNetSize; ++i)
		invPermInner[permInner[i]] = i;

	/*
	 * cautam sa vedem ce switch-uri vor fi activate in partea
	 * inferioara a retelei
	 */
	vector<char> path(subNetSize, -1);
	if (subNetSize % 2 == 1)
	{
		path[subNetSize - 1] = 1;
		path[permInner[subNetSize - 1]] = 1;
		if (permInner[subNetSize - 1] != subNetSize - 1)
		{
			int idx = permInner[invPermInner[subNetSize - 1] ^ 1];
			DFS(idx, 0, path);
		}
	}

	for (i = 0; i < subNetSize; ++i)
	{
		if (path[i] < 0)
		{
			DFS(i, 0, path);
		}
	}

	/*
	 * calculam noile perechi sursa-destinatie
	 * 1 pentru partea superioara
	 * 2 pentru partea inferioara
	 */
	// partea superioara
	for (i = 0; i < subNetSize - 1; i += 2)
	{
		mSwitches[depth][permIdx + i / 2] = path[i];
		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ path[i], coDepth);
			if (x < subNetSize / 2)
				bottom1.push_back(src[i | j]);
			else
				top1.push_back(src[i | j]);
		}
	}
	if (subNetSize % 2 == 1)
	{
		top1.push_back(src[subNetSize - 1]);
	}

	// partea inferioara
	for (i = 0; i < subNetSize - 1; i += 2)
	{
		s = mSwitches[depth + levels - 1][permIdx + i / 2] = path[permInner[i]];
		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, coDepth);
			if (x < subNetSize / 2)
				bottom2[x] = src[permInner[i | j]];
			else
			{
				top2[i / 2] = src[permInner[i | j]];
			}
		}
	}

	int idx = int(ceil(subNetSize * 0.5));
	if (subNetSize % 2 == 1)
	{
		top2[idx - 1] = dest[subNetSize - 1];
	}

	/*
	 * recursivitate prin partea superioara si inferioara
	 */
	genBenesRouteInner(depth + 1, permIdx, bottom1, bottom2);
	genBenesRouteInner(depth + 1, permIdx + subNetSize / 4, top1, top2);
}

void Benes::benesEval(vector<int> &src, int depth, int permIdx)
{
	int i, j, x, s;
	vector<int> bottom1;
	vector<int> top1;
	int subNetSize = src.size();
	int coDepth = mLogN - depth;
	int temp;

	if (subNetSize == 2)
	{
		if (coDepth == 1)
		{
			if (mSwitches[depth][permIdx] == 1)
			{
				temp = src[0];
				src[0] = src[1];
				src[1] = temp;
			}
		}
		else if (mSwitches[depth + 1][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}
		return;
	}

	if (subNetSize == 3)
	{
		if (mSwitches[depth][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}
		if (mSwitches[depth + 1][permIdx] == 1)
		{
			temp = src[1];
			src[1] = src[2];
			src[2] = temp;
		}
		if (mSwitches[depth + 2][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}
		return;
	}

	int levels = 2 * coDepth - 1;

	// partea superioara
	for (i = 0; i < subNetSize - 1; i += 2)
	{
		int s = mSwitches[depth][permIdx + i / 2];
		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, coDepth);
			if (x < subNetSize / 2)
				bottom1.push_back(src[i | j]);
			else
				top1.push_back(src[i | j]);
		}
	}
	if (subNetSize % 2 == 1)
	{
		top1.push_back(src[subNetSize - 1]);
	}

	benesEval(bottom1, depth + 1, permIdx);
	benesEval(top1, depth + 1, permIdx + subNetSize / 4);

	// partea inferioara
	for (i = 0; i < subNetSize - 1; i += 2)
	{
		s = mSwitches[depth + levels - 1][permIdx + i / 2];
		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, coDepth);
			if (x < subNetSize / 2)
				src[i | j] = bottom1[x];
			else
			{
				src[i | j] = top1[i / 2];
			}
		}
	}

	int idx = int(ceil(subNetSize * 0.5));
	if (subNetSize % 2 == 1)
	{
		src[subNetSize - 1] = top1[idx - 1];
	}
}

void Benes::genBenesRoute()
{
	vector<int> src(mN);
	std::iota(src.begin(), src.end(), 0);
	genBenesRouteInner(0, 0, src, mPerm);
}

void Benes::benesMaskedEval(vector<oc::block> &src,
														vector<vector<array<oc::block, 2>>> &otMsgs,
														int depth, int permIdx)
{
	int levels, i, j, x, s;
	vector<oc::block> bottom1;
	vector<oc::block> top1;
	int subNetSize = src.size();
	int coDepth = mLogN - depth;
	oc::block temp, temp_int[2];
	std::array<oc::block, 2> temp_block;

	if (subNetSize == 2)
	{
		if (coDepth == 1)
		{
			temp_block = otMsgs[depth][permIdx];
			memcpy(temp_int, &temp_block, sizeof(temp_int));
			src[0] = src[0] ^ temp_int[0];
			src[1] = src[1] ^ temp_int[1];
			if (mSwitches[depth][permIdx] == 1)
			{
				temp = src[0];
				src[0] = src[1];
				src[1] = temp;
			}
		}
		else
		{
			temp_block = otMsgs[depth + 1][permIdx];
			memcpy(temp_int, temp_block.data(), sizeof(temp_int));
			src[0] = src[0] ^ temp_int[0];
			src[1] = src[1] ^ temp_int[1];
			if (mSwitches[depth + 1][permIdx] == 1)
			{
				temp = src[0];
				src[0] = src[1];
				src[1] = temp;
			}
		}
		return;
	}

	if (subNetSize == 3)
	{
		temp_block = otMsgs[depth][permIdx];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[0] = src[0] ^ temp_int[0];
		src[1] = src[1] ^ temp_int[1];
		if (mSwitches[depth][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}

		temp_block = otMsgs[depth + 1][permIdx];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[1] = src[1] ^ temp_int[0];
		src[2] = src[2] ^ temp_int[1];
		if (mSwitches[depth + 1][permIdx] == 1)
		{
			temp = src[1];
			src[1] = src[2];
			src[2] = temp;
		}

		temp_block = otMsgs[depth + 2][permIdx];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[0] = src[0] ^ temp_int[0];
		src[1] = src[1] ^ temp_int[1];
		if (mSwitches[depth + 2][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}

		return;
	}

	levels = 2 * coDepth - 1;

	// partea superioara
	for (i = 0; i < subNetSize - 1; i += 2)
	{
		int s = mSwitches[depth][permIdx + i / 2];

		temp_block = otMsgs[depth][permIdx + i / 2];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));

		src[i] = src[i] ^ temp_int[0];
		src[i ^ 1] = src[i ^ 1] ^ temp_int[1]; //  ^ or | ??

		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, coDepth);
			if (x < subNetSize / 2)
			{
				bottom1.push_back(src[i | j]);
			}
			else
			{
				top1.push_back(src[i | j]);
			}
		}
	}
	if (subNetSize % 2 == 1)
	{
		top1.push_back(src[subNetSize - 1]);
	}

	benesMaskedEval(bottom1, otMsgs, depth + 1, permIdx);
	benesMaskedEval(top1, otMsgs, depth + 1, permIdx + subNetSize / 4);

	for (i = 0; i < subNetSize - 1; i += 2)
	{
		s = mSwitches[depth + levels - 1][permIdx + i / 2];

		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, coDepth);
			if (x < subNetSize / 2)
				src[i | j] = bottom1[x];
			else
			{
				src[i | j] = top1[i / 2];
			}
		}

		temp_block = otMsgs[depth + levels - 1][permIdx + i / 2];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[i] = src[i] ^ temp_int[s];
		src[i ^ 1] = src[i ^ 1] ^ temp_int[1 - s];
	}

	int idx = int(ceil(subNetSize * 0.5));
	if (subNetSize % 2 == 1)
	{
		src[subNetSize - 1] = top1[idx - 1];
	}
}

void Benes::benesMaskedEval(oc::BitVector &src,
														std::vector<std::vector<std::array<bool, 2>>> &otMsgs,
														int depth, int permIdx)
{
	int levels, i, j, x, s;
	oc::BitVector bottom1;
	oc::BitVector top1;
	int subNetSize = src.size();
	int coDepth = mLogN - depth;
	bool temp, temp_int[2];
	std::array<bool, 2> temp_block;

	if (subNetSize == 2)
	{
		if (coDepth == 1)
		{
			temp_block = otMsgs[depth][permIdx];
			memcpy(temp_int, &temp_block, sizeof(temp_int));
			src[0] = src[0] ^ temp_int[0];
			src[1] = src[1] ^ temp_int[1];
			if (mSwitches[depth][permIdx] == 1)
			{
				temp = src[0];
				src[0] = src[1];
				src[1] = temp;
			}
		}
		else
		{
			temp_block = otMsgs[depth + 1][permIdx];
			memcpy(temp_int, temp_block.data(), sizeof(temp_int));
			src[0] = src[0] ^ temp_int[0];
			src[1] = src[1] ^ temp_int[1];
			if (mSwitches[depth + 1][permIdx] == 1)
			{
				temp = src[0];
				src[0] = src[1];
				src[1] = temp;
			}
		}
		return;
	}

	if (subNetSize == 3)
	{
		temp_block = otMsgs[depth][permIdx];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[0] = src[0] ^ temp_int[0];
		src[1] = src[1] ^ temp_int[1];
		if (mSwitches[depth][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}

		temp_block = otMsgs[depth + 1][permIdx];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[1] = src[1] ^ temp_int[0];
		src[2] = src[2] ^ temp_int[1];
		if (mSwitches[depth + 1][permIdx] == 1)
		{
			temp = src[1];
			src[1] = src[2];
			src[2] = temp;
		}

		temp_block = otMsgs[depth + 2][permIdx];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[0] = src[0] ^ temp_int[0];
		src[1] = src[1] ^ temp_int[1];
		if (mSwitches[depth + 2][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}

		return;
	}

	levels = 2 * coDepth - 1;

	// partea superioara
	for (i = 0; i < subNetSize - 1; i += 2)
	{
		int s = mSwitches[depth][permIdx + i / 2];

		temp_block = otMsgs[depth][permIdx + i / 2];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));

		src[i] = src[i] ^ temp_int[0];
		src[i ^ 1] = src[i ^ 1] ^ temp_int[1]; //  ^ or | ??

		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, coDepth);
			if (x < subNetSize / 2)
			{
				bottom1.pushBack(src[i | j]);
			}
			else
			{
				top1.pushBack(src[i | j]);
			}
		}
	}
	if (subNetSize % 2 == 1)
	{
		top1.pushBack(src[subNetSize - 1]);
	}

	benesMaskedEval(bottom1, otMsgs, depth + 1, permIdx);
	benesMaskedEval(top1, otMsgs, depth + 1, permIdx + subNetSize / 4);

	for (i = 0; i < subNetSize - 1; i += 2)
	{
		s = mSwitches[depth + levels - 1][permIdx + i / 2];

		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, coDepth);
			if (x < subNetSize / 2)
				src[i | j] = bottom1[x];
			else
			{
				src[i | j] = top1[i / 2];
			}
		}

		temp_block = otMsgs[depth + levels - 1][permIdx + i / 2];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[i] = src[i] ^ temp_int[s];
		src[i ^ 1] = src[i ^ 1] ^ temp_int[1 - s];
	}

	int idx = int(ceil(subNetSize * 0.5));
	if (subNetSize % 2 == 1)
	{
		src[subNetSize - 1] = top1[idx - 1];
	}
}

oc::BitVector Benes::getSwitchesAsBitVec()
{
	oc::BitVector stretchedSwitches(mNumColumns * (mN / 2));
	for (int j = 0; j < mNumColumns; ++j)
		for (int i = 0; i < mN / 2; ++i)
		{
			stretchedSwitches[(mN / 2) * j + i] = mSwitches[j][i];
		}
	return stretchedSwitches;
}