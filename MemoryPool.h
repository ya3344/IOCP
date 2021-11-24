#pragma once

template <class Object>
class MemoryPool
{
public:
	struct BlockNodeInfo
	{
	public:
		bool AllocBlockNode(const bool isPlacementNew)
		{
			if (isPlacementNew == true)
			{
				data = (Object *)malloc(sizeof(Object));
				_ASSERT(data != nullptr);
				return true;
			}
			else
			{
				data = new Object;
				_ASSERT(data != nullptr);
				return true;
			}

		}
		void Release(const bool isPlacementNew)
		{
			if (isPlacementNew == true)
			{
				if (data)
				{
					free(data);
					data = nullptr;
				}
			}
			else
			{
				if (data)
				{
					delete data;
					data = nullptr;
				}
			}
		}

	public:
		Object* data = nullptr;
		BlockNodeInfo* next = nullptr;
	};

public:
	explicit MemoryPool(int blockNum, bool isPlacementNew = false)
	{
		mIsPlacementNew = isPlacementNew;

		for (int i = 0; i < blockNum; ++i)
		{
			BlockNodeInfo* newNode = new BlockNodeInfo;
			_ASSERT(newNode != nullptr);
	
			if (newNode->AllocBlockNode(mIsPlacementNew) == true)
			{
				newNode->next = mTopNode;
				mTopNode = newNode;
				// 할당한 개수 체크
				++mAllocCount;
			}
		}
	}
	virtual ~MemoryPool()
	{
		BlockNodeInfo* deleteNode = mTopNode;
		BlockNodeInfo* tempNode = nullptr;

		while (deleteNode != nullptr)
		{
			tempNode = deleteNode;
			deleteNode = deleteNode->next;

			// 오브젝트 삭제
			tempNode->Release(mIsPlacementNew);
			// 노드 삭제
			if(tempNode)
			{
				delete tempNode;
				tempNode = nullptr;
			}
			// 할당한 개수 감소
			--mAllocCount;
		}
	}
	
public:
	// 블럭 할당 함수
	BlockNodeInfo* Alloc(void)
	{
		BlockNodeInfo* newNode = nullptr;

		if (mTopNode == nullptr)
		{
			newNode = new BlockNodeInfo;
			_ASSERT(newNode != nullptr);

			// freelist인 경우에는 기본 생성자 호출 유도
			if (newNode->AllocBlockNode(mIsPlacementNew) == true)
			{
				newNode->next = mTopNode;
				mTopNode = newNode;
				// 할당한 개수 체크
				++mAllocCount;
			}
			else
				return nullptr;
		}

		mFreeNode = mTopNode;
		mTopNode = mFreeNode->next;

		// 사용중인 개수 체크
		++mUseCount;
		_ASSERT(mFreeNode != nullptr);

		// placement boolean 값이 참인 경우에만 생성자 호출
		if(mIsPlacementNew == true)
			new (mFreeNode->data) Object();

		return mFreeNode;
	};
	// 블럭 반환 삼수
	bool Free(BlockNodeInfo* blockNode)
	{
		_ASSERT(blockNode != nullptr);

		blockNode->next = mTopNode;
		mTopNode = blockNode;

		// placement boolean 값이 참인 경우에만 소멸자 호출
		if (mIsPlacementNew == true)
			blockNode->data->~Object();

		// 사용중인 개수 감소
		--mUseCount;

		return true;
	};

public:
	int GetAllockCount() const { return mAllocCount; }
	int GetUseCount() const { return mUseCount; }

private:
	BlockNodeInfo* mTopNode = nullptr;
	BlockNodeInfo* mFreeNode = nullptr;
	int mAllocCount = 0;
	int mUseCount = 0;
	bool mIsPlacementNew = false;
};

