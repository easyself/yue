// Copyright 2016 Cheng Zhao. All rights reserved.
// Use of this source code is governed by the license that can be found in the
// LICENSE file.

#include <string>

#include "lua/lua.h"
#include "testing/gtest/include/gtest/gtest.h"

class MetaTableTest : public testing::Test {
 protected:
  void SetUp() override {
    lua::SetTop(state_, 0);
  }

  lua::ManagedState state_;
};

class TestClass : public base::RefCounted<TestClass> {
 public:
  TestClass() : ptr_(nullptr) {}

  int Method1(int n) {
    return n;
  }

  std::string Method2(const char* str) {
    return str;
  }

  void SetPtr(int* ptr) {
    ptr_ = ptr;
  }

 protected:
  friend class base::RefCounted<TestClass>;
  virtual ~TestClass() {
    if (ptr_)
      *ptr_ = 456;
  }

 private:
  int* ptr_;

  DISALLOW_COPY_AND_ASSIGN(TestClass);
};

namespace lua {

template<>
struct Type<TestClass> {
  static constexpr const char* name = "TestClass";
  static void BuildMetaTable(State* state, int index) {
    RawSet(state, index,
           "new", &MetaTable<TestClass>::NewInstance<>,
           "method1", &TestClass::Method1,
           "method2", &TestClass::Method2);
  }
};

}  // namespace lua

TEST_F(MetaTableTest, PushNull) {
  lua::MetaTable<TestClass>::Push(state_);
  TestClass* instance = nullptr;
  lua::Push(state_, instance);
  EXPECT_EQ(lua::GetType(state_, -1), lua::LuaType::Nil);
}

TEST_F(MetaTableTest, PushNewClass) {
  lua::MetaTable<TestClass>::Push(state_);
  EXPECT_EQ(lua::GetTop(state_), 1);
  EXPECT_EQ(lua::GetType(state_, -1), lua::LuaType::Table);
  lua::RawGet(state_, -1, "method1", "method2", "__gc", "__index");
  EXPECT_EQ(lua::GetType(state_, -1), lua::LuaType::Table);
  EXPECT_EQ(lua::GetType(state_, -2), lua::LuaType::Function);
  EXPECT_EQ(lua::GetType(state_, -3), lua::LuaType::Function);
  EXPECT_EQ(lua::GetType(state_, -4), lua::LuaType::Function);
}

TEST_F(MetaTableTest, PushNewInstanceInC) {
  lua::MetaTable<TestClass>::Push(state_);
  lua::PopAndIgnore(state_, 1);
  lua::Push(state_, lua::MetaTable<TestClass>::NewInstance(state_));
  EXPECT_EQ(lua::GetTop(state_), 1);
  EXPECT_EQ(lua::GetType(state_, 1), lua::LuaType::UserData);
  lua::GetMetaTable(state_, 1);
  EXPECT_EQ(lua::GetTop(state_), 2);
  EXPECT_EQ(lua::GetType(state_, 2), lua::LuaType::Table);

  ASSERT_TRUE(lua::PGet(state_, 1, "method1"));
  int result = 456;
  ASSERT_TRUE(lua::PCall(state_, &result, lua::ValueOnStack(state_, 1), 123));
  ASSERT_EQ(result, 123);
}

TEST_F(MetaTableTest, PushNewInstanceInLua) {
  lua::MetaTable<TestClass>::Push(state_);
  ASSERT_TRUE(lua::PGet(state_, 1, "new"));
  TestClass* instance;
  EXPECT_TRUE(lua::PCall(state_, &instance));
  EXPECT_NE(instance, nullptr);

  lua::Push(state_, instance);
  ASSERT_TRUE(lua::PGet(state_, 1, "method2"));
  std::string result;
  ASSERT_TRUE(lua::PCall(state_, &result, instance, 123));
  ASSERT_EQ(result, "123");
}

TEST_F(MetaTableTest, GC) {
  lua::MetaTable<TestClass>::Push(state_);
  TestClass* instance = lua::MetaTable<TestClass>::NewInstance(state_);

  int changed = 123;
  instance->SetPtr(&changed);

  lua::CollectGarbage(state_);
  ASSERT_EQ(changed, 456);
}

TEST_F(MetaTableTest, GCWithCReference) {
  lua::MetaTable<TestClass>::Push(state_);
  scoped_refptr<TestClass> instance =
      lua::MetaTable<TestClass>::NewInstance(state_);

  int changed = 123;
  instance->SetPtr(&changed);

  lua::CollectGarbage(state_);
  ASSERT_EQ(changed, 123);
  instance = nullptr;
  ASSERT_EQ(changed, 456);
}

class DerivedClass : public TestClass {
 public:
  DerivedClass(int a, const char* b) : a_(a), b_(b) {}

  int A() const { return a_; }
  std::string B() const { return b_; }

 protected:
  ~DerivedClass() override {}

 private:
  int a_;
  std::string b_;
};

namespace lua {

template<>
struct Type<DerivedClass> {
  using base = TestClass;
  static constexpr const char* name = "DerivedClass";
  static void BuildMetaTable(State* state, int index) {
    RawSet(state, index,
           "new", &MetaTable<DerivedClass>::NewInstance<int, const char*>,
           "a", &DerivedClass::A,
           "b", &DerivedClass::B);
  }
};

}  // namespace lua

TEST_F(MetaTableTest, IndependentDerivedClass) {
  lua::MetaTable<DerivedClass>::Push(state_);
  lua::PopAndIgnore(state_, 1);
  lua::Push(state_, lua::MetaTable<DerivedClass>::NewInstance(state_, 1, "b"));
  EXPECT_EQ(lua::GetType(state_, 1), lua::LuaType::UserData);

  ASSERT_TRUE(lua::PGet(state_, 1, "a"));
  int a = 0;
  ASSERT_TRUE(lua::PCall(state_, &a, lua::ValueOnStack(state_, 1)));
  ASSERT_EQ(a, 1);

  ASSERT_TRUE(lua::PGet(state_, 1, "b"));
  std::string b;
  ASSERT_TRUE(lua::PCall(state_, &b, lua::ValueOnStack(state_, 1)));
  ASSERT_EQ(b, "b");
}

TEST_F(MetaTableTest, DerivedClassInheritanceChain) {
  lua::MetaTable<DerivedClass>::Push(state_);
  EXPECT_EQ(lua::GetTop(state_), 1);
  std::string name;
  ASSERT_TRUE(lua::RawGetAndPop(state_, 1, "__name", &name));
  ASSERT_EQ(name, "DerivedClass");
  ASSERT_TRUE(lua::GetMetaTable(state_, -1));
  EXPECT_EQ(lua::GetTop(state_), 2);
  EXPECT_EQ(lua::GetType(state_, 2), lua::LuaType::Table);
  lua::RawGet(state_, -1, "__index");
  EXPECT_EQ(lua::GetTop(state_), 3);
  EXPECT_EQ(lua::GetType(state_, 3), lua::LuaType::Table);
  ASSERT_TRUE(lua::RawGetAndPop(state_, 3, "__name", &name));
  ASSERT_EQ(name, "TestClass");
}

TEST_F(MetaTableTest, DerivedClassMethods) {
  lua::MetaTable<DerivedClass>::Push(state_);
  DerivedClass* instance =
      lua::MetaTable<DerivedClass>::NewInstance(state_, 456, "str");
  lua::Push(state_, instance);
  EXPECT_EQ(lua::GetTop(state_), 2);
  ASSERT_TRUE(lua::PGet(state_, 2, "a", "b", "method1", "method2"));
  EXPECT_EQ(lua::GetType(state_, -1), lua::LuaType::Function);
  EXPECT_EQ(lua::GetType(state_, -2), lua::LuaType::Function);
  EXPECT_EQ(lua::GetType(state_, -3), lua::LuaType::Function);
  EXPECT_EQ(lua::GetType(state_, -4), lua::LuaType::Function);

  int result = 456;
  ASSERT_TRUE(lua::PGet(state_, 2, "method1"));
  ASSERT_TRUE(lua::PCall(state_, &result, lua::ValueOnStack(state_, 2), 123));
  EXPECT_EQ(result, 123);
  ASSERT_TRUE(lua::PGet(state_, 2, "a"));
  ASSERT_TRUE(lua::PCall(state_, &result, lua::ValueOnStack(state_, 2)));
  EXPECT_EQ(result, 456);
}

class DerivedClass2 : public DerivedClass {
 public:
  DerivedClass2() : DerivedClass(123, "test"), ptr_(nullptr) {}

  int C() const { return 789; }

  void SetPtr(int* ptr) {
    ptr_ = ptr;
  }

 private:
  ~DerivedClass2() override {
    if (ptr_)
      *ptr_ = 789;
  }

  int* ptr_;
};

namespace lua {

template<>
struct Type<DerivedClass2> {
  using base = DerivedClass;
  static constexpr const char* name = "DerivedClass2";
  static void BuildMetaTable(State* state, int index) {
    RawSet(state, index, "new", &MetaTable<DerivedClass2>::NewInstance<>,
                         "c", &DerivedClass2::C);
  }
};

}  // namespace lua

TEST_F(MetaTableTest, DeeplyDerivedClassInheritanceChain) {
  lua::MetaTable<DerivedClass2>::Push(state_);
  EXPECT_EQ(lua::GetTop(state_), 1);
  std::string name;
  ASSERT_TRUE(lua::RawGetAndPop(state_, 1, "__name", &name));
  ASSERT_EQ(name, "DerivedClass2");
  ASSERT_TRUE(lua::GetMetaTable(state_, -1));
  EXPECT_EQ(lua::GetTop(state_), 2);
  EXPECT_EQ(lua::GetType(state_, 2), lua::LuaType::Table);
  lua::RawGet(state_, -1, "__index");
  EXPECT_EQ(lua::GetTop(state_), 3);
  EXPECT_EQ(lua::GetType(state_, 3), lua::LuaType::Table);
  ASSERT_TRUE(lua::RawGetAndPop(state_, 3, "__name", &name));
  ASSERT_EQ(name, "DerivedClass");
  ASSERT_TRUE(lua::GetMetaTable(state_, -1));
  EXPECT_EQ(lua::GetTop(state_), 4);
  EXPECT_EQ(lua::GetType(state_, 4), lua::LuaType::Table);
  lua::RawGet(state_, -1, "__index");
  EXPECT_EQ(lua::GetTop(state_), 5);
  EXPECT_EQ(lua::GetType(state_, 5), lua::LuaType::Table);
  ASSERT_TRUE(lua::RawGetAndPop(state_, 5, "__name", &name));
  ASSERT_EQ(name, "TestClass");
}

TEST_F(MetaTableTest, DeeplyDerivedClassGC) {
  lua::MetaTable<DerivedClass2>::Push(state_);
  DerivedClass2* d2 = lua::MetaTable<DerivedClass2>::NewInstance(state_);
  int changed_d2 = 0;
  d2->SetPtr(&changed_d2);

  TestClass* b;
  lua::Push(state_, d2);
  ASSERT_TRUE(lua::To(state_, -1, &b));
  int changed_b = 0;
  b->SetPtr(&changed_b);

  lua::SetTop(state_, 0);
  lua::CollectGarbage(state_);
  EXPECT_EQ(changed_b, 456);
  EXPECT_EQ(changed_d2, 789);
}

TEST_F(MetaTableTest, BaseCallDerivedClassMethods) {
  lua::MetaTable<DerivedClass2>::Push(state_);
  TestClass* b = lua::MetaTable<TestClass>::NewInstance(state_);
  ASSERT_TRUE(lua::PGet(state_, 1, "a"));
  ASSERT_FALSE(lua::PCall(state_, nullptr, b, 123));
  std::string error;
  ASSERT_TRUE(lua::Pop(state_, &error));
  ASSERT_EQ(error, "error converting arg at index 1 "
                   "from TestClass to DerivedClass");
}

TEST_F(MetaTableTest, BaseConvertToDerivedClass) {
  lua::MetaTable<DerivedClass2>::Push(state_);
  lua::Push(state_, lua::MetaTable<TestClass>::NewInstance(state_));
  DerivedClass2* d2;
  ASSERT_FALSE(lua::Pop(state_, &d2));
}

TEST_F(MetaTableTest, PushWithoutNewInstance) {
  lua::MetaTable<DerivedClass2>::Push(state_);
  DerivedClass2* d2 = new DerivedClass2;
  lua::Push(state_, d2);
  TestClass* b;
  ASSERT_TRUE(lua::Pop(state_, &b));
  EXPECT_EQ(d2, b);
}

TEST_F(MetaTableTest, PushWithoutNewInstanceAfterGC) {
  lua::MetaTable<DerivedClass2>::Push(state_);
  lua::Push(state_, new DerivedClass2);

  TestClass* b;
  ASSERT_TRUE(lua::To(state_, -1, &b));
  int changed = 0;
  b->SetPtr(&changed);

  scoped_refptr<TestClass> keep(b);
  lua::SetTop(state_, 0);
  lua::CollectGarbage(state_);
  EXPECT_EQ(changed, 0);

  lua::Push(state_, keep.get());
  keep = nullptr;
  ASSERT_TRUE(lua::To(state_, -1, &b));
  EXPECT_NE(b, nullptr);

  lua::SetTop(state_, 0);
  lua::CollectGarbage(state_);
  EXPECT_EQ(changed, 456);
}

TEST_F(MetaTableTest, PushMultipleBaseClasses) {
  lua::MetaTable<DerivedClass2>::Push(state_);
  lua::Push(state_, new DerivedClass2);
  lua::Push(state_, new DerivedClass(123, "test"));
  lua::Push(state_, new TestClass);
  TestClass* b1;
  TestClass* b2;
  TestClass* b3;
  ASSERT_TRUE(lua::Pop(state_, &b1, &b2, &b3));

  int changed_1 = 0;
  int changed_2 = 0;
  int changed_3 = 0;
  scoped_refptr<TestClass> s1(b1);
  scoped_refptr<TestClass> s2(b2);
  scoped_refptr<TestClass> s3(b3);
  s1->SetPtr(&changed_1);
  s2->SetPtr(&changed_2);
  s3->SetPtr(&changed_3);

  lua::SetTop(state_, 0);
  lua::CollectGarbage(state_);

  EXPECT_EQ(changed_1, 0);
  EXPECT_EQ(changed_2, 0);
  EXPECT_EQ(changed_3, 0);

  lua::Push(state_, s1.get(), s2.get(), s3.get());
  s1 = nullptr;
  s2 = nullptr;
  s3 = nullptr;

  EXPECT_EQ(changed_1, 0);
  EXPECT_EQ(changed_2, 0);
  EXPECT_EQ(changed_3, 0);

  lua::SetTop(state_, 0);
  lua::CollectGarbage(state_);

  EXPECT_EQ(changed_1, 456);
  EXPECT_EQ(changed_2, 456);
  EXPECT_EQ(changed_3, 456);
}

TEST_F(MetaTableTest, ConvertedBaseWithoutParentMethods) {
  lua::MetaTable<DerivedClass2>::Push(state_);
  lua::Push(state_, new DerivedClass2);

  ASSERT_TRUE(lua::PGet(state_, 2, "a", "b", "c"));
  EXPECT_EQ(lua::GetType(state_, -1), lua::LuaType::Function);
  EXPECT_EQ(lua::GetType(state_, -2), lua::LuaType::Function);
  EXPECT_EQ(lua::GetType(state_, -3), lua::LuaType::Function);

  DerivedClass* d1;
  ASSERT_TRUE(lua::To(state_, 2, &d1));
  scoped_refptr<DerivedClass> keep(d1);
  lua::SetTop(state_, 0);
  lua::CollectGarbage(state_);

  lua::Push(state_, keep.get());
  ASSERT_TRUE(lua::PGet(state_, 1, "a", "b", "c"));
  EXPECT_EQ(lua::GetType(state_, -1), lua::LuaType::Nil);
  EXPECT_EQ(lua::GetType(state_, -2), lua::LuaType::Function);
  EXPECT_EQ(lua::GetType(state_, -3), lua::LuaType::Function);
}

class PropertiesClass : public base::RefCounted<PropertiesClass> {
 public:
  PropertiesClass() {}

  int property = 0;

 private:
  friend class base::RefCounted<PropertiesClass>;

  ~PropertiesClass() {}
};

namespace lua {

template<>
struct Type<PropertiesClass> {
  static constexpr const char* name = "PropertiesClass";
  static void BuildMetaTable(State* state, int index) {
    RawSet(state, index,
           "new", &MetaTable<PropertiesClass>::NewInstance<>);
  }
  static bool Index(State* state, const std::string& name) {
    PropertiesClass* self;
    if (!To(state, 1, &self))
      return false;
    if (name == "property") {
      Push(state, self->property);
      return true;
    } else {
      return false;
    }
  }
};

}  // namespace lua

TEST_F(MetaTableTest, Index) {
  lua::MetaTable<PropertiesClass>::Push(state_);
  lua::RawGet(state_, 1, "__index");
  EXPECT_EQ(lua::GetType(state_, -1), lua::LuaType::Function);
  lua::RawGet(state_, 1, "__newindex");
  EXPECT_EQ(lua::GetType(state_, -1), lua::LuaType::Nil);

  lua::Push(state_, new PropertiesClass);
  int property = -1;
  ASSERT_TRUE(lua::PGetAndPop(state_, -1, "property", &property));
  EXPECT_EQ(property, 0);
}

class NewIndexClass : public base::RefCounted<NewIndexClass> {
 public:
  NewIndexClass() {}

  int property = 0;

 private:
  friend class base::RefCounted<NewIndexClass>;

  ~NewIndexClass() {}
};

namespace lua {

template<>
struct Type<NewIndexClass> {
  static constexpr const char* name = "NewIndexClass";
  static void BuildMetaTable(State* state, int index) {
    RawSet(state, index,
           "new", &MetaTable<NewIndexClass>::NewInstance<>);
  }
  static bool Index(State* state, const std::string& name) {
    NewIndexClass* self;
    if (!To(state, 1, &self))
      return false;
    if (name == "property") {
      Push(state, self->property);
      return true;
    }
    return false;
  }
  static bool NewIndex(State* state, const std::string& name) {
    NewIndexClass* self;
    int value;
    if (!To(state, 1, &self) || !To(state, 3, &value))
      return false;
    if (name == "property") {
      self->property = value;
      return true;
    }
    return false;
  }
};

}  // namespace lua

TEST_F(MetaTableTest, NewIndex) {
  lua::MetaTable<NewIndexClass>::Push(state_);
  lua::RawGet(state_, 1, "__index");
  EXPECT_EQ(lua::GetType(state_, -1), lua::LuaType::Function);
  lua::RawGet(state_, 1, "__newindex");
  EXPECT_EQ(lua::GetType(state_, -1), lua::LuaType::Function);

  lua::Push(state_, new NewIndexClass);
  ASSERT_TRUE(lua::PSet(state_, -1, "property", 123));
  int property;
  ASSERT_TRUE(lua::PGetAndPop(state_, -1, "property", &property));
  EXPECT_EQ(property, 123);
}

class TestWeakPtrClass {
 public:
  TestWeakPtrClass() : weak_factory_(this) {}
  ~TestWeakPtrClass() {}

  int Method() { return 123; }

  base::WeakPtr<TestWeakPtrClass> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestWeakPtrClass> weak_factory_;
};

namespace lua {

template<>
struct Type<TestWeakPtrClass> {
  static constexpr const char* name = "TestWeakPtrClass";
  static void BuildMetaTable(State* state, int index) {
    RawSet(state, index, "method", &TestWeakPtrClass::Method);
  }
};

}  // namespace lua

TEST_F(MetaTableTest, WeakPtr) {
  lua::MetaTable<TestWeakPtrClass>::Push(state_);
  std::unique_ptr<TestWeakPtrClass> instance(new TestWeakPtrClass);

  lua::Push(state_, instance.get());
  TestWeakPtrClass* ptr;
  ASSERT_TRUE(lua::To(state_, -1, &ptr));
  EXPECT_EQ(ptr, instance.get());

  instance.reset();
  ASSERT_FALSE(lua::To(state_, -1, &ptr));
}
