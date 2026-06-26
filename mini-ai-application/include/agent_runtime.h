#ifndef AGENT_RUNTIME_H
#define AGENT_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>

#define AGENT_MAX_TOOLS    32
#define AGENT_MAX_STEPS    50
#define AGENT_MAX_MEMORY   256

typedef enum { TOOL_SEARCH, TOOL_CALC, TOOL_CODE, TOOL_API, TOOL_DB, TOOL_FILE } ToolType;

typedef struct {
    char name[64]; char description[256]; ToolType type;
    bool (*execute)(const char *input, char *output, uint32_t max_len);
} AgentTool;

typedef struct {
    char role[16]; char content[1024];
} AgentMessage;

typedef struct {
    AgentMessage history[AGENT_MAX_MEMORY];
    uint32_t msg_count;
    AgentTool tools[AGENT_MAX_TOOLS];
    uint32_t tool_count;
    uint32_t max_steps;
    char system_prompt[1024];
} AgentConfig;

typedef struct {
    AgentConfig config;
    uint32_t steps_taken;
    bool task_completed;
    char final_answer[2048];
} AgentRuntime;

/* L1 API */
void agent_init(AgentRuntime *agent);
void agent_set_prompt(AgentRuntime *agent, const char *prompt);
void agent_register_tool(AgentRuntime *agent, const char *name, const char *desc, ToolType type, bool (*exec)(const char*, char*, uint32_t));

/* L2: ReAct loop (Yao et al. 2023) — Reasoning + Acting */
typedef enum { ACTION_NONE, ACTION_REASON, ACTION_USE_TOOL, ACTION_ANSWER } AgentAction;
void agent_react_loop(AgentRuntime *agent, const char *task);
AgentAction agent_decide_action(const AgentRuntime *agent, const char *observation);

/* L5: Tool selection via keyword matching */
uint32_t agent_select_tool(const AgentRuntime *agent, const char *query);
void agent_add_message(AgentRuntime *agent, const char *role, const char *content);

/* L7: Multi-agent coordination */
typedef struct { AgentRuntime *agents; uint32_t count; } MultiAgentSystem;
void agent_multi_delegate(MultiAgentSystem *mas, const char *task, uint32_t *agent_id);

void agent_print_trace(const AgentRuntime *agent);

/* L8: Reflexion Pattern */
typedef struct { char reflection[1024]; double confidence; bool should_retry; uint32_t retry_count; } AgentReflection;
void agent_reflect(AgentRuntime *agent, const char *output, AgentReflection *reflection);

/* L8: Tree-of-Thought */
#define TOT_MAX_BRANCHES 5
#define TOT_MAX_DEPTH 3
#define TOT_BEAM_WIDTH 2
typedef struct { char thought[512]; double value; uint32_t depth; uint32_t parent_idx; } ToTNode;
typedef struct { ToTNode nodes[64]; uint32_t count; uint32_t best_idx; double best_value; } ToTState;
void agent_tot_init(ToTState *state);
void agent_tot_search(ToTState *state, const char *problem);

/* L9: Multi-agent Debate */
typedef struct { char answers[8][1024]; uint32_t answer_count; uint32_t votes[8]; } AgentDebate;
void agent_debate_init(AgentDebate *debate);
void agent_debate_add_answer(AgentDebate *debate, const char *answer);
void agent_debate_vote(AgentDebate *debate);
uint32_t agent_debate_winner(const AgentDebate *debate);

/* L9: Safety */
typedef enum { SAFE, FLAG_HARMFUL, FLAG_DISHONEST, FLAG_PRIVACY } SafetyLabel;
SafetyLabel agent_safety_check(const char *output);
const char *agent_safety_label_str(SafetyLabel label);

/* L5: UCB Tool Selection */
typedef struct { uint32_t uses; double total_reward; double mean_reward; double ucb_score; } ToolStats;
void agent_tool_stats_init(ToolStats *stats, uint32_t n);
uint32_t agent_select_tool_ucb(const AgentRuntime *agent, const char *query, ToolStats *stats);
void agent_tool_update_stats(ToolStats *stats, uint32_t tool_idx, double reward);

/* Utility */
void agent_clear_memory(AgentRuntime *agent);
void agent_get_conversation(const AgentRuntime *agent, char *buf, uint32_t max_len);
void agent_register_default_tools(AgentRuntime *agent);
#endif