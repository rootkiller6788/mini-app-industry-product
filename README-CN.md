# Mini App Industry Product

一组**从零构建、零外部依赖的 C 语言实现**，覆盖金融、医疗、制造、能源、农业、物流、电商、政务与科研等行业的工业级计算内核。每个子模块对标 MIT、Stanford 等顶尖高校课程，将真实世界的工业算法转化为可运行的 C 代码。

## 子模块

| 子模块 | 主题 | 对标课程 |
|--------|------|---------|
| [mini-agriculture-tech](mini-agriculture-tech/) | 精准农业、GPS 田块测绘、作物生长模型（GDD/NDVI）、灌溉调度、遥感分析、土壤建模、WGEN 天气生成 | MIT 6.004, Stanford CS 229, UC Davis, Wageningen |
| [mini-ai-application](mini-ai-application/) | NLP 流水线（BPE/TF-IDF/NER）、推荐系统（协同过滤/矩阵分解）、智能体运行时安全检测、推理服务器、AI 平台 | MIT 6.036, Stanford CS 224N, CS 229, CS 246, CS 329S |
| [mini-dev-tools-platform](mini-dev-tools-platform/) | CI/CD 流水线、CLI 解析器、代码审查、配置管理（热加载）、部署管理（金丝雀）、特性开关（A/B 灰度）、日志、包管理、性能分析、测试运行器 | MIT 6.824, Stanford CS 144, CS 245, CMU SEI |
| [mini-ecommerce-retail](mini-ecommerce-retail/) | 库存/订单管理、支付引擎、全文搜索（TF-IDF/BM25）、欺诈检测（贝叶斯）、事件溯源/CQRS、A/B 测试、秒杀、限流 | MIT 6.858, Stanford CS 144, Berkeley CS 186, CMU 15-445 |
| [mini-energy-carbon](mini-energy-carbon/) | 光伏建模、风电（Weibull 分布）、电池储能优化、碳排放核算（GHG Protocol/IPCC AR6）、电力市场、负荷预测（Holt-Winters）、微电网定容 | MIT 15.031J, Stanford CS 229, Berkeley CS 267, CMU 15-418 |
| [mini-enterprise-system](mini-enterprise-system/) | ERP 核心（MRP 引擎）、CRM 销售漏斗、财务报表、人力资源管理、企业资源计划 | MIT 6.004, SAP/Oracle University |
| [mini-fintech-blockchain](mini-fintech-blockchain/) | 复式记账法、Black-Scholes/Monte Carlo 期权定价、投资组合优化（Markowitz/CAPM）、风险管理（VaR）、债券定价（Nelson-Siegel）、Merkle 树区块链、AMM/CBDC | MIT 15.433, MIT 15.450, Stanford MS&E 245G, ETH 363-0537 |
| [mini-future-computing](mini-future-computing/) | 量子仿真、神经形态计算、光子计算、概率（p-bit）计算、热力学可逆计算、生物/DNA 计算、后摩尔范式 | MIT, Caltech, Nature 2016 |
| [mini-gov-smartcity](mini-gov-smartcity/) | 市民 311 门户、服务请求路由、应急管理、交通管理、环境监测、基础设施建模、ISO 37120/UN SDG 11 | MIT 11.520, NYU CUSP |
| [mini-healthcare-medical](mini-healthcare-medical/) | 临床决策支持（DSS）、医院运营（ADT）、检验分析仪、患者监护（NEWS2）、药房管理、药物相互作用检测 | MIT HST.542J, Stanford MED 277, BIOMEDIN 215, Harvard BMI 703 |
| [mini-industry-solution](mini-industry-solution/) | 工业过程控制（PID）、PLC 协议（Modbus）、预测性维护、生产排程、质量控制（SPC）、安全分析（LOPA）、ISA-88/ISA-95、SCADA | MIT 2.171, Stanford EE 264, Berkeley EECS 149, IEOR 160 |
| [mini-internet-app](mini-internet-app/) | HTTP/1.1 Web 服务器、REST API（JSON）、URL 路由、内容缓存、会话认证、限流器、模板引擎 | MIT 6.033, Stanford CS 142 |
| [mini-logistics-supplychain](mini-logistics-supplychain/) | 车辆路径规划（TSP/VRP）、需求预测、库存优化（EOQ/报童模型）、仓储管理、运输、供应链动态（牛鞭效应）、绿色物流 | MIT 15.762, MIT CTL.SCx, MIT 15.053, Stanford MS&E 246 |
| [mini-scientific-research](mini-scientific-research/) | 实验生命周期、数值方法、最优化、线性代数、统计学、数据溯源、FAIR 数据原则、可复现性、元分析 | Stanford, Cambridge |
| [mini-smart-manufacturing](mini-smart-manufacturing/) | MES/SCADA 核心、OEE 监控、生产调度（Johnson 规则）、质量控制（SPC）、智能制造、供应链集成 | Georgia Tech ISyE, RWTH Aachen |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **自包含模块** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`、`docs/`
- **工业算法内核** — 每个模块实现真实的计算核心（定价引擎、物理模型、优化求解器），而非 CRUD 封装
- **跨模块集成** — 金融引用 AI 预测，制造集成 IoT 传感器，电商对接安全风控引擎

## 构建

每个模块独立运行。进入模块目录执行：

```bash
cd mini-agriculture-tech
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-app-industry-product/
├── mini-agriculture-tech/         # 精准农业、作物建模、遥感分析
├── mini-ai-application/           # NLP、推荐系统、智能体运行时、AI 平台
├── mini-dev-tools-platform/       # CI/CD、日志、性能分析、配置管理、特性开关
├── mini-ecommerce-retail/         # 库存、支付、搜索、欺诈检测、CQRS
├── mini-energy-carbon/            # 光伏、风电、电池、碳核算、电力市场
├── mini-enterprise-system/        # ERP（MRP、财务）、CRM、HRM
├── mini-fintech-blockchain/       # 期权定价、投资组合理论、风险、区块链、DeFi
├── mini-future-computing/         # 量子、神经形态、光子、热力学计算
├── mini-gov-smartcity/            # 智慧城市治理、311 门户、交通/应急管理
├── mini-healthcare-medical/       # 临床 DSS、医院运营、检验分析、患者监护
├── mini-industry-solution/        # ISA-95/88、PLC、SCADA、预测性维护、SPC
├── mini-internet-app/             # HTTP 服务器、REST API、缓存、认证、路由
├── mini-logistics-supplychain/    # VRP、库存优化、仓储、供应链动态
├── mini-scientific-research/      # 数值方法、最优化、FAIR 数据、可复现性
└── mini-smart-manufacturing/      # MES、OEE、生产调度、SPC、智能工厂
```

## 许可证

MIT
