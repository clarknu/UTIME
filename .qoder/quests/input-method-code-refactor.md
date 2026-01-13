# UTIME 输入法代码重构设计

## 文档元信息

| 属性 | 内容 |
|------|------|
| 创建时间 | 2026-01-13 |
| 重构范围 | 全代码库结构优化 |
| 重构目标 | 清理冗余代码、统一代码结构、提升可维护性 |
| 风险级别 | 中等（保留所有核心功能） |

---

## 一、重构背景与动机

### 1.1 当前状态

UTIME输入法经过多次迭代修复，已实现核心功能：
- ✅ 拼音输入与候选词显示
- ✅ 数字键、空格键、上下键选择候选词
- ✅ 多策略光标定位（5层容错）
- ✅ 多路径数据库初始化（3级回退）
- ✅ 组合状态管理与文字上屏
- ✅ 模糊拼音与纠错功能

### 1.2 存在的问题

通过代码审查发现以下问题：

| 问题类别 | 具体表现 | 影响 |
|---------|---------|------|
| **冗余代码** | 注释掉的旧代码未清理（DictionaryEngine.cpp 第287-292行） | 降低可读性 |
| **重复逻辑** | 光标定位策略代码冗长（TextService.cpp 第500-643行） | 维护困难 |
| **魔法数字** | 窗口样式、偏移量、限制值硬编码 | 缺乏灵活性 |
| **日志过度** | 关键路径日志过多（平均每函数5-10条） | 性能影响 |
| **职责不清** | TextService类既管理输入又管理UI | 违反单一职责 |
| **错误处理不统一** | 部分函数返回HRESULT，部分返回bool | 风格不一致 |

### 1.3 重构目标

| 目标 | 衡量标准 |
|------|---------|
| **代码精简** | 删除冗余代码，减少10-15%代码行数 |
| **结构清晰** | 每个类职责明确，降低圈复杂度 |
| **可配置化** | 魔法数字提取为配置常量 |
| **日志优化** | 保留关键路径日志，移除调试日志 |
| **一致性** | 统一命名风格、错误处理模式 |

---

## 二、代码审查分析

### 2.1 文件级别分析

#### 核心文件健康度评估

| 文件 | 当前行数 | 职责 | 健康度 | 主要问题 |
|------|---------|------|--------|---------|
| `TextService.cpp` | 752行 | 输入处理、UI管理、光标定位 | 🟡 中等 | 职责过多、函数过长 |
| `DictionaryEngine.cpp` | 387行 | 词库查询、模糊拼音 | 🟢 良好 | 有少量注释代码 |
| `CandidateWindow.cpp` | 227行 | 候选窗口UI | 🟢 良好 | 结构合理 |
| `EditSession.cpp` | 247行 | TSF编辑会话 | 🟢 良好 | 结构清晰 |

#### 问题热点

```
TextService.cpp
├── DebugLog函数 (8-57行) - 可提取到独立模块
├── OnKeyDown函数 (203-410行) - 208行函数体，需拆分
│   ├── 字母键处理 (215-222行)
│   ├── 退格键处理 (223-238行)
│   ├── 数字键处理 (239-280行) - 逻辑重复
│   ├── 空格键处理 (282-328行) - 逻辑重复
│   ├── 回车键处理 (330-369行) - 逻辑重复
│   ├── 上下键处理 (371-400行)
│   └── ESC键处理 (401-407行)
└── _UpdateCandidateWindow函数 (480-688行) - 209行函数体，过长
    ├── 策略1: TSF GetTextExt (509-530行)
    ├── 策略2: GetGUIThreadInfo + 窗口句柄 (533-585行)
    ├── 策略3: GetGUIThreadInfo + 前台窗口 (587-615行)
    ├── 策略4: GetCaretPos (618-639行)
    └── 策略5: 鼠标位置回退 (664-678行)
```

### 2.2 代码气味识别

#### 长函数（Long Method）

| 函数 | 行数 | 建议 |
|------|-----|------|
| `CTextService::OnKeyDown` | 208行 | 拆分为按键类型处理器 |
| `CTextService::_UpdateCandidateWindow` | 209行 | 提取光标定位策略模式 |
| `CDictionaryEngine::Initialize` | 147行 | 提取路径查找和验证逻辑 |

#### 重复代码（Duplicate Code）

**数字键、空格键、回车键的候选词提交逻辑高度相似**：
```
模式：
1. 确定要提交的文本
2. 创建CCommitCompositionEditSession
3. 请求同步编辑会话
4. 处理TF_E_SYNCHRONOUS回退
5. 记录日志
6. 清理状态（_sComposition、_candidateList、候选窗口）
```

重复次数：3次（数字键、空格键、回车键）

#### 魔法数字（Magic Numbers）

| 位置 | 值 | 含义 | 建议 |
|------|---|------|------|
| TextService.cpp:669 | 20 | 鼠标位置Y轴偏移 | 提取为配置常量 |
| CandidateWindow.cpp:100 | 25 | 候选窗口Y轴偏移 | 提取为配置常量 |
| CandidateWindow.cpp:84 | 200 | 候选窗口最小宽度 | 提取为配置常量 |
| DictionaryEngine.cpp:321 | 5 | 模糊变体最大数量 | 提取为配置常量 |
| DictionaryEngine.cpp:338 | 20 | SQL查询结果限制 | 提取为配置常量 |

#### 注释代码（Commented Code）

| 位置 | 内容 | 建议 |
|------|-----|------|
| DictionaryEngine.h:20-21 | 已废弃的LoadFromTextFile、InsertRecord声明 | 删除 |
| DictionaryEngine.cpp:287-292 | 已废弃的InsertRecord注释 | 删除 |
| Register.cpp:75-79 | Display Attributes注释 | 删除或文档化 |

---

## 三、重构方案设计

### 3.1 架构重构

#### 3.1.1 职责分离原则

**当前架构问题**：
- `CTextService` 承担了输入处理、UI管理、光标定位、候选词提交等多重职责

**重构后架构**：

```
┌──────────────────────────────────────────────────────┐
│                   CTextService                       │
│  - 核心职责：TSF生命周期管理、按键路由              │
│  - 委托给：键盘处理器、候选词管理器                 │
└────────────────┬─────────────────────────────────────┘
                 │
                 ├─────> CKeyboardHandler (新增)
                 │       - 按键事件分发
                 │       - 组合状态管理
                 │
                 ├─────> CCandidateManager (新增)
                 │       - 候选词查询
                 │       - 候选词选择逻辑
                 │       - 候选窗口协调
                 │
                 └─────> CCaretPositionStrategy (新增)
                         - 封装5种光标定位策略
                         - 策略链模式

┌──────────────────────────────────────────────────────┐
│              CDictionaryEngine                       │
│  - 保持当前职责：词库查询、模糊拼音                 │
└──────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────┐
│              CCandidateWindow                        │
│  - 保持当前职责：候选窗口UI渲染                      │
└──────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────┐
│              CEditSessionBase + 派生类               │
│  - 保持当前职责：TSF编辑会话管理                     │
└──────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────┐
│              CLogger (新增)                          │
│  - 统一日志管理                                      │
│  - 日志级别控制                                      │
└──────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────┐
│              Config (新增)                           │
│  - 配置常量集中管理                                  │
└──────────────────────────────────────────────────────┘
```

#### 3.1.2 新增模块职责定义

| 模块 | 职责 | 公开接口 |
|------|------|---------|
| **CKeyboardHandler** | 按键处理逻辑封装 | `OnKeyEvent(WPARAM, LPARAM)` <br> `HandleAlphaKey(wchar_t)` <br> `HandleNumberKey(int)` <br> `HandleBackspace()` <br> `HandleSpace()` <br> `HandleEnter()` <br> `HandleEscape()` |
| **CCandidateManager** | 候选词生命周期管理 | `UpdateCandidates(const std::wstring&)` <br> `SelectCandidate(int)` <br> `CommitCandidate(int)` <br> `GetCurrentCandidates()` |
| **CCaretPositionStrategy** | 光标位置获取策略 | `GetCaretPosition(ITfContext*)` <br> （返回POINT或错误） |
| **CLogger** | 日志管理 | `LogInfo(format, ...)` <br> `LogDebug(format, ...)` <br> `LogError(format, ...)` <br> `SetLogLevel(level)` |

### 3.2 代码优化策略

#### 3.2.1 提取候选词提交公共逻辑

**问题**：数字键、空格键、回车键存在重复的提交逻辑

**解决方案**：提取为独立的辅助方法

**方法签名**：
```
函数名称: _CommitCandidateText
参数:
  - ITfContext* pContext: 目标上下文
  - const std::wstring& text: 要提交的文本
返回值: HRESULT
职责:
  1. 创建CCommitCompositionEditSession
  2. 请求同步编辑会话，失败则回退异步
  3. 记录日志
  4. 返回执行结果
```

**调用点改造**：
- 数字键处理：`_CommitCandidateText(pic, _candidateList[index])`
- 空格键处理：`_CommitCandidateText(pic, selectedCandidate)`
- 回车键处理：`_CommitCandidateText(pic, _sComposition)`

#### 3.2.2 光标定位策略模式

**问题**：`_UpdateCandidateWindow` 函数过长（209行）

**解决方案**：采用责任链模式封装5种策略

**类设计**：

```
接口: ICaretStrategy
├── GetPosition(ITfContext*, ITfContextView*) -> RECT
└── GetName() -> std::wstring

实现类:
├── TSFGetTextExtStrategy (策略1)
├── GUIThreadInfoWindowStrategy (策略2)
├── GUIThreadInfoForegroundStrategy (策略3)
├── GetCaretPosStrategy (策略4)
└── MouseCursorFallbackStrategy (策略5)

管理类: CCaretPositionStrategy
├── _strategies: vector<ICaretStrategy*>
├── GetCaretPosition(ITfContext*) -> POINT
│   └── 遍历策略链，返回第一个成功的结果
└── AddStrategy(ICaretStrategy*)
```

**职责分离**：
- `_UpdateCandidateWindow` 只负责：查询候选词 + 显示窗口
- 光标定位逻辑完全委托给 `CCaretPositionStrategy`

#### 3.2.3 配置常量提取

**新增文件**: `include/Config.h`

**内容结构**：

| 配置类别 | 常量名称 | 值 | 说明 |
|---------|---------|---|------|
| **候选窗口配置** | `CANDIDATE_WINDOW_MIN_WIDTH` | 200 | 最小宽度 |
|  | `CANDIDATE_WINDOW_Y_OFFSET` | 25 | Y轴偏移 |
|  | `CANDIDATE_WINDOW_PADDING` | 5 | 内边距 |
|  | `CANDIDATE_LINE_HEIGHT` | 24 | 行高 |
| **光标定位配置** | `MOUSE_FALLBACK_Y_OFFSET` | 20 | 鼠标回退Y偏移 |
|  | `CARET_FALLBACK_WIDTH` | 2 | 光标默认宽度 |
|  | `CARET_FALLBACK_HEIGHT` | 20 | 光标默认高度 |
| **词库查询配置** | `MAX_FUZZY_VARIANTS` | 5 | 模糊变体数量限制 |
|  | `MAX_QUERY_RESULTS` | 20 | SQL查询结果限制 |
| **日志配置** | `LOG_FILE_PATH` | `C:\\Windows\\Temp\\UTIME_Debug.log` | 日志文件路径 |
|  | `DEFAULT_LOG_LEVEL` | `LOG_LEVEL_INFO` | 默认日志级别 |

#### 3.2.4 日志系统重构

**问题**：日志分散在各处，缺乏级别控制

**解决方案**：封装为 `CLogger` 类

**日志级别定义**：

| 级别 | 用途 | 输出条件 |
|-----|------|---------|
| `LOG_LEVEL_ERROR` | 严重错误（如数据库打开失败） | 始终输出 |
| `LOG_LEVEL_WARN` | 警告（如策略失败但有备用方案） | 生产环境输出 |
| `LOG_LEVEL_INFO` | 关键流程（如按键事件、文字提交） | 生产环境输出 |
| `LOG_LEVEL_DEBUG` | 调试详情（如每个策略尝试） | 仅开发环境 |

**日志精简原则**：
- 删除所有"函数进入"类的日志（如 `DoEditSession called`）
- 保留错误、警告、关键操作成功/失败日志
- 调试日志使用宏控制编译（Release版本不包含）

**实现方式**：

```
类名: CLogger

成员:
- _logLevel: 当前日志级别
- _cs: CRITICAL_SECTION（线程安全）
- _outputToFile: bool（是否写入文件）
- _outputToDebugger: bool（是否输出到调试器）

方法:
- GetInstance() -> CLogger& （单例）
- SetLogLevel(level) -> void
- LogError(format, ...) -> void
- LogWarn(format, ...) -> void
- LogInfo(format, ...) -> void
- LogDebug(format, ...) -> void （仅Debug编译包含）

私有方法:
- _WriteLog(level, message) -> void
```

#### 3.2.5 代码清理清单

| 清理项 | 位置 | 操作 |
|-------|------|------|
| **删除注释代码** | `DictionaryEngine.h:20-21` | 删除废弃函数声明 |
|  | `DictionaryEngine.cpp:287-292` | 删除注释的实现 |
|  | `DictionaryEngine.cpp:68-69` | 删除注释的宏定义 |
| **删除冗余日志** | `EditSession.cpp` 全文 | 删除 "called" 类日志（约6条） |
|  | `TextService.cpp:_UpdateCandidateWindow` | 删除每个策略的详细日志（约15条） |
|  | `DictionaryEngine.cpp:Query` | 删除绑定参数日志（第354行） |
| **统一命名风格** | 全局 | 确保成员变量使用 `_` 前缀 |
|  | 全局 | 确保私有方法使用 `_` 前缀 |
| **移除硬编码** | 见3.2.3节 | 替换为配置常量 |

---

## 四、重构实施计划

### 4.1 分阶段重构策略

#### 阶段1：无风险清理（优先级：P0）

**目标**：删除明确无用的代码，不影响任何功能

| 任务 | 文件 | 工作量 | 风险 |
|-----|------|--------|------|
| 删除注释代码 | DictionaryEngine.h/cpp | 5分钟 | 🟢 无 |
| 删除冗余日志 | TextService.cpp, EditSession.cpp | 15分钟 | 🟢 无 |
| 提取配置常量 | 新增Config.h，修改使用处 | 30分钟 | 🟢 无 |

**验证标准**：编译通过 + 基础输入测试通过

#### 阶段2：局部重构（优先级：P1）

**目标**：提取公共逻辑，降低代码重复

| 任务 | 文件 | 工作量 | 风险 |
|-----|------|--------|------|
| 提取候选词提交方法 | TextService.cpp | 45分钟 | 🟡 低 |
| 日志系统封装 | 新增Logger.h/cpp | 60分钟 | 🟡 低 |
| 更新日志调用 | 全局 | 30分钟 | 🟡 低 |

**验证标准**：编译通过 + 完整功能测试（数字键、空格、回车提交）

#### 阶段3：架构重构（优先级：P2）

**目标**：职责分离，提升可维护性

| 任务 | 文件 | 工作量 | 风险 |
|-----|------|--------|------|
| 实现光标定位策略模式 | 新增CaretPositionStrategy.h/cpp | 120分钟 | 🟡 中 |
| 重构_UpdateCandidateWindow | TextService.cpp | 30分钟 | 🟡 中 |
| 实现CCandidateManager | 新增CandidateManager.h/cpp | 90分钟 | 🟡 中 |
| 实现CKeyboardHandler | 新增KeyboardHandler.h/cpp | 90分钟 | 🟡 中 |
| 重构TextService | TextService.h/cpp | 60分钟 | 🟡 中 |

**验证标准**：编译通过 + 全场景测试（记事本、Chrome、Word）

### 4.2 测试策略

#### 回归测试检查点

| 功能点 | 测试场景 | 预期结果 |
|-------|---------|---------|
| **基础输入** | 输入 `nihao` + 空格 | 输出"你好" |
|  | 输入 `zhongguo` + 回车 | 输出"zhongguo" |
|  | 输入 `abc` + ESC | 取消输入 |
| **候选词选择** | 输入 `hao` + 数字键2 | 选择第2个候选词 |
|  | 输入 `hao` + 上下键 + 空格 | 选择高亮候选词 |
|  | 输入 `hao` + 鼠标点击候选词 | 选择点击候选词 |
| **光标定位** | 在记事本中输入 | 候选窗口贴近光标 |
|  | 在Chrome搜索框输入 | 候选窗口不遮挡文字 |
|  | 在Word中输入 | 候选窗口位置正确 |
| **异常处理** | 快速输入并删除 | 不崩溃 |
|  | 切换窗口时有输入 | 状态正确清理 |

#### 性能基准测试

| 指标 | 测量方法 | 基准值 | 重构后目标 |
|-----|---------|--------|-----------|
| **按键响应时间** | 从按键到候选窗口显示 | < 50ms | 保持或优化 |
| **候选词查询时间** | 数据库查询耗时 | < 10ms | 保持 |
| **内存占用** | 进程工作集 | ~5MB | 保持 |

---

## 五、风险评估与缓解

### 5.1 技术风险

| 风险项 | 概率 | 影响 | 缓解措施 |
|-------|------|------|---------|
| **重构引入功能回退** | 中 | 高 | 每阶段完整回归测试 |
| **编译错误** | 低 | 中 | 增量重构，及时编译验证 |
| **性能下降** | 低 | 中 | 保留性能关键路径的直接调用 |
| **日志系统线程安全问题** | 低 | 中 | 使用CRITICAL_SECTION保护 |

### 5.2 回滚策略

**分支管理**：
```
main (生产分支)
  └── refactor/cleanup (阶段1分支)
        └── refactor/extract-common (阶段2分支)
              └── refactor/architecture (阶段3分支)
```

**回滚决策点**：
- 每阶段测试失败 → 回滚到上一阶段
- 发现重大bug → 立即回滚到main分支
- 性能下降超过20% → 回滚并重新评估

---

## 六、代码结构对比

### 6.1 重构前后文件结构

#### 重构前（当前）

```
include/
├── CandidateWindow.h
├── DictionaryEngine.h
├── EditSession.h
├── GetTextExtEditSession.h (内联在头文件)
├── Globals.h
└── TextService.h

src/
├── CandidateWindow.cpp (227行)
├── DictionaryEngine.cpp (387行)
├── EditSession.cpp (247行)
├── Register.cpp (125行)
├── TextService.cpp (752行) ⚠️ 过长
├── dllmain.cpp (114行)
└── UTIME.def
```

#### 重构后（预期）

```
include/
├── CandidateWindow.h (保持)
├── DictionaryEngine.h (精简)
├── EditSession.h (保持)
├── Globals.h (保持)
├── TextService.h (精简)
├── Config.h ⭐ 新增
├── Logger.h ⭐ 新增
├── CaretPositionStrategy.h ⭐ 新增 (可选P2)
├── CandidateManager.h ⭐ 新增 (可选P2)
└── KeyboardHandler.h ⭐ 新增 (可选P2)

src/
├── CandidateWindow.cpp (~220行，精简)
├── DictionaryEngine.cpp (~320行，精简)
├── EditSession.cpp (~200行，精简)
├── Register.cpp (~120行，精简)
├── TextService.cpp (~400行，大幅精简) ⭐
├── dllmain.cpp (保持)
├── Logger.cpp ⭐ 新增
├── CaretPositionStrategy.cpp ⭐ 新增 (可选P2)
├── CandidateManager.cpp ⭐ 新增 (可选P2)
├── KeyboardHandler.cpp ⭐ 新增 (可选P2)
└── UTIME.def (保持)
```

### 6.2 代码行数对比（预估）

| 阶段 | 总行数 | 变化 | 说明 |
|-----|--------|-----|------|
| **重构前** | ~2200行 | - | 当前状态 |
| **阶段1完成** | ~1950行 | -250行 | 删除冗余，提取配置 |
| **阶段2完成** | ~2050行 | +100行 | 日志系统封装（新增代码） |
| **阶段3完成** | ~2400行 | +350行 | 架构重构（职责分离，新增模块） |

**注意**：虽然阶段3总行数增加，但单文件复杂度显著降低，可维护性提升。

---

## 七、成功标准

### 7.1 功能完整性

- ✅ 所有现有功能正常工作
- ✅ 性能指标无明显下降（响应时间、内存占用）
- ✅ 无新增崩溃或异常

### 7.2 代码质量

| 指标 | 目标值 | 测量方法 |
|-----|--------|---------|
| **单文件最大行数** | < 400行 | 直接统计 |
| **单函数最大行数** | < 80行 | 直接统计 |
| **注释代码行数** | 0行 | 代码审查 |
| **魔法数字数量** | < 5个 | 代码审查 |
| **日志数量（生产级别）** | 减少30% | 日志文件大小对比 |

### 7.3 可维护性

- ✅ 新增功能时，代码改动点明确（单一职责）
- ✅ 文档完整（设计文档、代码注释）
- ✅ 新成员能在1小时内理解架构

---

## 八、后续优化建议

### 短期（重构完成后1周内）

1. **性能基准测试**：对比重构前后的响应时间和资源占用
2. **用户测试**：在真实场景中验证各种应用兼容性
3. **代码审查**：团队成员交叉审查重构代码

### 中期（1-2个月）

1. **单元测试补充**：为关键模块添加单元测试（DictionaryEngine、CandidateManager）
2. **集成测试自动化**：自动化按键输入和结果验证
3. **文档完善**：更新README，添加开发者指南

### 长期（3-6个月）

1. **配置文件支持**：将Config.h改为从配置文件读取
2. **插件化架构**：支持第三方词库、皮肤
3. **性能优化**：实现候选词缓存、数据库连接池

---

## 九、附录

### 9.1 关键决策记录

| 决策 | 理由 | 权衡 |
|-----|------|------|
| **保留现有日志文件路径** | 避免用户日志收集流程变更 | 灵活性降低 |
| **不删除GetTextExtEditSession.h** | 代码简洁，无必要拆分 | 与其他头文件风格不统一 |
| **阶段3作为可选** | 架构重构风险较高，可延后 | 短期可维护性提升有限 |

### 9.2 技术债务清单

以下问题在本次重构中不解决，记录为技术债务：

| 债务项 | 说明 | 优先级 |
|-------|------|--------|
| **单元测试缺失** | 核心模块无自动化测试 | 高 |
| **硬编码语言ID** | Register.cpp中0x0804硬编码 | 中 |
| **错误处理不完整** | 部分COM调用未检查HRESULT | 中 |
| **内存泄漏检测** | 未使用工具检测潜在泄漏 | 低 |
| **Display Attributes未实现** | Register.cpp注释表明功能未完成 | 低 |

### 9.3 参考资料

- **TSF官方文档**: [Microsoft TSF Documentation](https://docs.microsoft.com/en-us/windows/win32/tsf)
- **代码重构经典**: Martin Fowler《重构：改善既有代码的设计》
- **设计模式**: GoF《设计模式：可复用面向对象软件的基础》
- **项目历史文档**: `FIX_SUMMARY.md`, `TESTING_GUIDE.md`

---

## 十、置信度评估

| 评估维度 | 置信度 | 依据 |
|---------|--------|------|
| **需求理解** | 高 | 完整审查现有代码，明确当前问题 |
| **技术方案** | 高 | 基于成熟的重构模式（策略模式、责任链） |
| **实施可行性** | 中-高 | 阶段1/2风险低，阶段3需谨慎测试 |
| **时间估算** | 中 | 阶段1+2约3小时，阶段3约7小时 |

**关键假设**：
- 假设1：现有功能是正确的，重构不改变行为
- 假设2：测试环境覆盖主要使用场景（记事本、Chrome）
- 假设3：有充足时间进行回归测试

**风险提示**：
- ⚠️ 阶段3架构重构涉及多个文件修改，需要额外谨慎
- ⚠️ 日志系统改造可能影响现有问题诊断流程
- ⚠️ 建议优先完成阶段1和阶段2，验证稳定后再考虑阶段3
