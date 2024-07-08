#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <stack>
#include <fstream>
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Common/BitVector.h>
#include "benes.h"

using namespace std;
using namespace oc;

void Benes::initialize(int N)
{
	mN = N;
	mLogN = int(ceil(log2(N)));
	mNumColumns = 2 * mLogN - 1;
	
	perm.resize(N);
	invPerm.resize(N);
	switched.resize(mNumColumns);
	for (int i = 0; i < mNumColumns; ++i)
		switched[i].resize(N / 2);
}

void Benes::DFS(int idx, int route, vector<char>& path)
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

		idx = perm[invPerm[pr.first] ^ 1];
		if (path[idx] < 0)
			st.push({idx, pr.second ^ 1});
	}
}

int shuffle(int i, int n)
{
	return ((i & 1) << (n - 1)) | (i >> 1);
}

void Benes::genBenesRoute(int colEnd, int colStart, int permIdx, const vector<int> &src,
														const vector<int> &dest, bool verbose)
{
	int i, j, x, s;
	vector<int> bottom1;
	vector<int> top1;
	int subNetSize = src.size();

	/*
	 * daca avem doar un nivel in retea
	 */
	if (subNetSize == 2)
	{
		if (colEnd == 1)
			switched[colStart][permIdx] = src[0] != dest[0];
		else
			switched[colStart + 1][permIdx] = src[0] != dest[0];
		return;
	}

	if (subNetSize == 3)
	{
		if (src[0] == dest[0])
		{
			switched[colStart][permIdx] = 0;
			switched[colStart + 2][permIdx] = 0;
			if (src[1] == dest[1])
				switched[colStart + 1][permIdx] = 0;
			else
				switched[colStart + 1][permIdx] = 1;
		}

		if (src[0] == dest[1])
		{
			switched[colStart][permIdx] = 0;
			switched[colStart + 2][permIdx] = 1;
			if (src[1] == dest[0])
				switched[colStart + 1][permIdx] = 0;
			else
				switched[colStart + 1][permIdx] = 1;
		}

		if (src[0] == dest[2])
		{
			switched[colStart][permIdx] = 1;
			switched[colStart + 1][permIdx] = 1;
			if (src[1] == dest[0])
				switched[colStart + 2][permIdx] = 0;
			else
				switched[colStart + 2][permIdx] = 1;
		}
		return;
	}

	/*
	 * aflam dimensiunea retelei benes
	 */
	int levels = 2 * colEnd - 1;

	vector<int> bottom2(subNetSize / 2);
	vector<int> top2(int(ceil(subNetSize * 0.5)));

	/*
	 * destinatia este o permutare a intrari
	 */

	for (i = 0; i < subNetSize; ++i)
		invPerm[src[i]] = i;

	for (i = 0; i < subNetSize; ++i)
		perm[i] = invPerm[dest[i]];

	for (i = 0; i < subNetSize; ++i)
		invPerm[perm[i]] = i;

	/*
	 * cautam sa vedem ce switch-uri vor fi activate in partea
	 * inferioara a retelei
	 */
	vector<char> path(subNetSize, -1);
	if (subNetSize % 2 == 1)
	{
		path[subNetSize - 1] = 1;
		path[perm[subNetSize - 1]] = 1;
		if (perm[subNetSize - 1] != subNetSize - 1)
		{
			int idx = perm[invPerm[subNetSize - 1] ^ 1];
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
		switched[colStart][permIdx + i / 2] = path[i];
		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ path[i], colEnd);
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
		s = switched[colStart + levels - 1][permIdx + i / 2] = path[perm[i]];
		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, colEnd);
			if (x < subNetSize / 2)
				bottom2[x] = src[perm[i | j]];
			else
			{
				top2[i / 2] = src[perm[i | j]];
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
	genBenesRoute(colEnd - 1, colStart + 1, permIdx, bottom1, bottom2);
	genBenesRoute(colEnd - 1, colStart + 1, permIdx + subNetSize / 4, top1, top2);
}

void Benes::benesEval(int colEnd, int colStart, int permIdx, vector<int> &src)
{
	int i, j, x, s;
	vector<int> bottom1;
	vector<int> top1;
	int subNetSize = src.size();
	int temp;

	if (subNetSize == 2)
	{
		if (colEnd == 1)
		{
			if (switched[colStart][permIdx] == 1)
			{
				temp = src[0];
				src[0] = src[1];
				src[1] = temp;
			}
		}
		else if (switched[colStart + 1][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}
		return;
	}

	if (subNetSize == 3)
	{
		if (switched[colStart][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}
		if (switched[colStart + 1][permIdx] == 1)
		{
			temp = src[1];
			src[1] = src[2];
			src[2] = temp;
		}
		if (switched[colStart + 2][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}
		return;
	}

	int levels = 2 * colEnd - 1;

	// partea superioara
	for (i = 0; i < subNetSize - 1; i += 2)
	{
		int s = switched[colStart][permIdx + i / 2];
		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, colEnd);
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

	benesEval(colEnd - 1, colStart + 1, permIdx, bottom1);
	benesEval(colEnd - 1, colStart + 1, permIdx + subNetSize / 4, top1);

	// partea inferioara
	for (i = 0; i < subNetSize - 1; i += 2)
	{
		s = switched[colStart + levels - 1][permIdx + i / 2];
		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, colEnd);
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

void Benes::benesMaskedEval(int colEnd, int colStart, int permIdx, vector<oc::block> &src,
	vector<vector<array<osuCrypto::block, 2>>> &otMsgs)
{
	int levels, i, j, x, s;
	vector<oc::block> bottom1;
	vector<oc::block> top1;
	int subNetSize = src.size();
	oc::block temp, temp_int[2];
	std::array<oc::block, 2> temp_block;

	if (subNetSize == 2)
	{
		if (colEnd == 1)
		{
			temp_block = otMsgs[colStart][permIdx];
			memcpy(temp_int, &temp_block, sizeof(temp_int));
			src[0] = src[0] ^ temp_int[0];
			src[1] = src[1] ^ temp_int[1];
			if (switched[colStart][permIdx] == 1)
			{
				temp = src[0];
				src[0] = src[1];
				src[1] = temp;
			}
		}
		else
		{
			temp_block = otMsgs[colStart + 1][permIdx];
			memcpy(temp_int, temp_block.data(), sizeof(temp_int));
			src[0] = src[0] ^ temp_int[0];
			src[1] = src[1] ^ temp_int[1];
			if (switched[colStart + 1][permIdx] == 1)
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
		temp_block = otMsgs[colStart][permIdx];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[0] = src[0] ^ temp_int[0];
		src[1] = src[1] ^ temp_int[1];
		if (switched[colStart][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}

		temp_block = otMsgs[colStart + 1][permIdx];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[1] = src[1] ^ temp_int[0];
		src[2] = src[2] ^ temp_int[1];
		if (switched[colStart + 1][permIdx] == 1)
		{
			temp = src[1];
			src[1] = src[2];
			src[2] = temp;
		}

		temp_block = otMsgs[colStart + 2][permIdx];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));
		src[0] = src[0] ^ temp_int[0];
		src[1] = src[1] ^ temp_int[1];
		if (switched[colStart + 2][permIdx] == 1)
		{
			temp = src[0];
			src[0] = src[1];
			src[1] = temp;
		}

		return;
	}

	levels = 2 * colEnd - 1;

	// partea superioara
	for (i = 0; i < subNetSize - 1; i += 2)
	{
		int s = switched[colStart][permIdx + i / 2];

		temp_block = otMsgs[colStart][permIdx + i / 2];
		memcpy(temp_int, temp_block.data(), sizeof(temp_int));

		src[i] = src[i] ^ temp_int[0];
		src[i ^ 1] = src[i ^ 1] ^ temp_int[1]; //  ^ or | ??

		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, colEnd);
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

	benesMaskedEval(colEnd - 1, colStart + 1, permIdx, bottom1, otMsgs);
	benesMaskedEval(colEnd - 1, colStart + 1, permIdx + subNetSize / 4, top1, otMsgs);

	for (i = 0; i < subNetSize - 1; i += 2)
	{
		s = switched[colStart + levels - 1][permIdx + i / 2];

		for (j = 0; j < 2; ++j)
		{
			x = shuffle((i | j) ^ s, colEnd);
			if (x < subNetSize / 2)
				src[i | j] = bottom1[x];
			else
			{
				src[i | j] = top1[i / 2];
			}
		}

		temp_block = otMsgs[colStart + levels - 1][permIdx + i / 2];
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

osuCrypto::BitVector Benes::getSwitches()
{
	osuCrypto::BitVector switches(mN * mNumColumns / 2);
	for (int j = 0; j < mNumColumns; ++j)
		for (int i = 0; i < mN / 2; ++i)
		{
			switches[(mN * j) / 2 + i] = switched[j][i];
		}
	return switches;
}