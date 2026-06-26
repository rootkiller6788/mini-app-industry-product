/* ============================================================================
 * agent_runtime.c — AI Agent Runtime (ReAct Loop Implementation)
 *
 * L1: Core Definitions — AgentRuntime, AgentTool, AgentConfig
 * L2: ReAct Loop (Yao et al. 2023) — Reasoning + Acting interleaved
 * L5: Tool selection via keyword matching (BM25-inspired scoring)
 * L7: Multi-agent coordination with delegation
 * L8: Chain-of-Thought reasoning simulation
 * L9: Agent memory management (conversation history, working memory)
 *
 * Reference:
 * - Yao et al. 2023 "ReAct: Synergizing Reasoning and Acting in Language Models"
 * - Wei et al. 2022 "Chain-of-Thought Prompting Elicits Reasoning in LLMs"
 * - Shinn et al. 2023 "Reflexion: Language Agents with Verbal Reinforcement Learning"
 * ========================================================================== */

#include "agent_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>

/* ─── L1: Core API ───────────────────────────────────────────────── */

void agent_init(AgentRuntime *agent)
{
    if (!agent) return;
    memset(agent, 0, sizeof(*agent));
    agent->config.max_steps = AGENT_MAX_STEPS;
    strcpy(agent->config.system_prompt,
           "You are a helpful AI assistant. Use tools when needed.");
}

void agent_set_prompt(AgentRuntime *agent, const char *prompt)
{
    if (!agent || !prompt) return;
    strncpy(agent->config.system_prompt, prompt, 1023);
    agent->config.system_prompt[1023] = '\0';
}

void agent_register_tool(AgentRuntime *agent, const char *name,
                          const char *desc, ToolType type,
                          bool (*exec)(const char *, char *, uint32_t))
{
    if (!agent || !name || agent->config.tool_count >= AGENT_MAX_TOOLS) return;
    AgentTool *t = &agent->config.tools[agent->config.tool_count];
    strncpy(t->name, name, 63);
    if (desc) strncpy(t->description, desc, 255);
    t->type = type;
    t->execute = exec;
    agent->config.tool_count++;
}

void agent_add_message(AgentRuntime *agent, const char *role, const char *content)
{
    if (!agent || !role || !content) return;
    if (agent->config.msg_count >= AGENT_MAX_MEMORY) return;
    AgentMessage *m = &agent->config.history[agent->config.msg_count];
    strncpy(m->role, role, 15);
    strncpy(m->content, content, 1023);
    m->content[1023] = '\0';
    agent->config.msg_count++;
}

/* ─── L2: ReAct Loop ─────────────────────────────────────────────── */

static bool agent_tool_search(const char *input, char *output, uint32_t max_len)
{
    (void)input;
    snprintf(output, max_len, "Search results for query: 3 relevant documents found.");
    return true;
}

static bool agent_tool_calc(const char *input, char *output, uint32_t max_len)
{
    double result = 0.0;
    int a = 0, b = 0;
    char op = '+';
    if (sscanf(input, "%d %c %d", &a, &op, &b) >= 3) {
        switch (op) {
        case '+': result = (double)a + (double)b; break;
        case '-': result = (double)a - (double)b; break;
        case '*': result = (double)a * (double)b; break;
        case '/':
            if (b != 0) result = (double)a / (double)b;
            break;
        default: break;
        }
    }
    snprintf(output, max_len, "%.6f", result);
    return true;
}

static bool agent_tool_code(const char *input, char *output, uint32_t max_len)
{
    snprintf(output, max_len, "Code execution result for: %s", input);
    return true;
}

/* Register default tools */
void agent_register_default_tools(AgentRuntime *agent)
{
    agent_register_tool(agent, "search", "Search the internet for information",
                        TOOL_SEARCH, agent_tool_search);
    agent_register_tool(agent, "calculator", "Perform arithmetic calculations",
                        TOOL_CALC, agent_tool_calc);
    agent_register_tool(agent, "code_exec", "Execute Python code snippets",
                        TOOL_CODE, agent_tool_code);
}

/* ─── L5: Tool Selection ─────────────────────────────────────────── */

#ifndef strcasestr
static const char *strcasestr_portable(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return NULL;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        size_t i;
        for (i = 0; i < nlen; i++) {
            if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i]))
                break;
        }
        if (i == nlen) return haystack;
    }
    return NULL;
}
#define strcasestr strcasestr_portable
#endif

uint32_t agent_select_tool(const AgentRuntime *agent, const char *query)
{
    if (!agent || !query || agent->config.tool_count == 0) return 0;

    /* Simple keyword matching: score each tool by word overlap */
    int best_score = -1;
    uint32_t best_idx = 0;

    for (uint32_t i = 0; i < agent->config.tool_count; i++) {
        int score = 0;
        const char *desc = agent->config.tools[i].description;
        const char *name = agent->config.tools[i].name;

        /* Score by word matches in description */
        char query_copy[256];
        strncpy(query_copy, query, 255);
        query_copy[255] = '\0';

        char *word = strtok(query_copy, " ,.!?");
        while (word) {
            if (strcasestr(desc, word) || strcasestr(name, word)) {
                score += (int)strlen(word);
            }
            word = strtok(NULL, " ,.!?");
        }
        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }
    return (best_score > 0) ? best_idx : 0;
}

/* ─── L8: Chain-of-Thought Reasoning ─────────────────────────────── */

static void agent_think(AgentRuntime *agent, const char *observation,
                         char *thought, uint32_t max_len)
{
    if (!agent || !observation || !thought) return;

    /* Simulated chain-of-thought:
     * 1. Parse observation
     * 2. Identify what's missing
     * 3. Decide next action
     */
    if (agent->config.msg_count == 0) {
        snprintf(thought, max_len,
                 "I need to understand the task. Let me search for relevant information first.");
    } else if (strstr(observation, "found") || strstr(observation, "result")) {
        snprintf(thought, max_len,
                 "I have gathered some information. Let me analyze it and formulate an answer.");
    } else {
        snprintf(thought, max_len,
                 "Let me think about what tool to use next to complete the task.");
    }
}

AgentAction agent_decide_action(const AgentRuntime *agent,
                                 const char *observation)
{
    if (!agent || !observation) return ACTION_NONE;

    /* Decision logic:
     * - If task just started (no history): REASON first
     * - If observation contains "error" or nothing useful: USE_TOOL
     * - If step count approaching max: ANSWER
     * - Default: REASON */
    if (agent->config.msg_count == 0) return ACTION_REASON;

    if (strstr(observation, "error") || strstr(observation, "not found"))
        return ACTION_USE_TOOL;

    if (agent->steps_taken >= agent->config.max_steps - 1)
        return ACTION_ANSWER;

    /* Check if answer can be derived */
    bool has_enough_info = (agent->steps_taken >= 3);
    return has_enough_info ? ACTION_ANSWER : ACTION_USE_TOOL;
}

/* ─── L7: ReAct Loop Main Execution ──────────────────────────────── */

void agent_react_loop(AgentRuntime *agent, const char *task)
{
    if (!agent || !task) return;

    agent->steps_taken = 0;
    agent->task_completed = false;
    memset(agent->final_answer, 0, sizeof(agent->final_answer));

    /* Register default tools if none */
    if (agent->config.tool_count == 0) {
        agent_register_default_tools(agent);
    }

    agent_add_message(agent, "system", agent->config.system_prompt);
    agent_add_message(agent, "user", task);

    char observation[1024] = "Task received.";
    char thought[512];
    char tool_output[1024];

    while (agent->steps_taken < agent->config.max_steps && !agent->task_completed) {
        agent->steps_taken++;

        /* Step 1: Think */
        agent_think(agent, observation, thought, sizeof(thought));
        agent_add_message(agent, "thought", thought);

        /* Step 2: Decide action */
        AgentAction action = agent_decide_action(agent, observation);

        switch (action) {
        case ACTION_USE_TOOL: {
            /* Select best tool */
            uint32_t tool_idx = agent_select_tool(agent, observation);
            if (tool_idx < agent->config.tool_count) {
                AgentTool *tool = &agent->config.tools[tool_idx];
                char action_msg[512];
                snprintf(action_msg, sizeof(action_msg), "Using tool: %s", tool->name);
                agent_add_message(agent, "action", action_msg);

                bool ok = tool->execute(observation, tool_output, sizeof(tool_output));
                if (ok) {
                    strncpy(observation, tool_output, 1023);
                    observation[1023] = '\0';
                } else {
                    snprintf(observation, sizeof(observation),
                             "Tool %s failed to execute.", tool->name);
                }
                agent_add_message(agent, "observation", observation);
            }
            break;
        }
        case ACTION_ANSWER: {
            snprintf(agent->final_answer, sizeof(agent->final_answer),
                     "Based on my analysis, the answer to '%s' is: "
                     "After %u steps of reasoning and tool use, "
                     "I have gathered sufficient information to respond.",
                     task, agent->steps_taken);
            agent_add_message(agent, "assistant", agent->final_answer);
            agent->task_completed = true;
            break;
        }
        case ACTION_REASON:
            /* Just think more */
            break;
        default:
            break;
        }
    }

    /* Force answer if max steps reached */
    if (!agent->task_completed) {
        snprintf(agent->final_answer, sizeof(agent->final_answer),
                 "I was unable to complete the task within %u steps.",
                 agent->steps_taken);
        agent->task_completed = true;
    }
}

/* ─── L7: Multi-Agent Coordination ───────────────────────────────── */

void agent_multi_delegate(MultiAgentSystem *mas, const char *task,
                           uint32_t *agent_id)
{
    if (!mas || !task || !agent_id) return;

    /* Simple round-robin delegation with task suitability heuristic */
    uint32_t best = 0;
    uint32_t best_steps = UINT32_MAX;

    for (uint32_t i = 0; i < mas->count; i++) {
        /* Prefer less busy agent */
        if (mas->agents[i].steps_taken < best_steps) {
            best_steps = mas->agents[i].steps_taken;
            best = i;
        }
    }
    *agent_id = best;

    /* Delegate task */
    agent_react_loop(&mas->agents[best], task);
}

/* ─── L9: Agent Memory Management ────────────────────────────────── */

void agent_clear_memory(AgentRuntime *agent)
{
    if (!agent) return;
    agent->config.msg_count = 0;
    agent->steps_taken = 0;
    agent->task_completed = false;
    memset(agent->final_answer, 0, sizeof(agent->final_answer));
}

void agent_get_conversation(const AgentRuntime *agent, char *buf, uint32_t max_len)
{
    if (!agent || !buf || max_len == 0) return;
    buf[0] = '\0';
    size_t pos = 0;
    for (uint32_t i = 0; i < agent->config.msg_count && pos < max_len - 1; i++) {
        const AgentMessage *m = &agent->config.history[i];
        int n = snprintf(buf + pos, max_len - pos, "[%s] %s\n", m->role, m->content);
        if (n < 0) break;
        pos += (size_t)n;
    }
}

void agent_print_trace(const AgentRuntime *agent)
{
    if (!agent) return;
    printf("\n=== Agent Execution Trace ===\n");
    printf("Steps: %u | Completed: %s\n",
           agent->steps_taken, agent->task_completed ? "yes" : "no");
    printf("Tools registered: %u\n", agent->config.tool_count);
    printf("Messages: %u\n\n", agent->config.msg_count);
    for (uint32_t i = 0; i < agent->config.msg_count; i++) {
        const AgentMessage *m = &agent->config.history[i];
        printf("[%s] %s\n", m->role, m->content);
    }
    if (agent->final_answer[0]) {
        printf("\n--- Final Answer ---\n%s\n", agent->final_answer);
    }
}

/* L8: Reflexion Pattern (Shinn et al. 2023)
 * Agent self-critiques its output and retries with improved reasoning.
 * Key insight: LLM agents improve through verbal reinforcement —
 * the agent reflects on failures and adjusts its strategy. */

void agent_reflect(AgentRuntime *agent, const char *output,
                    AgentReflection *reflection)
{
    if (!agent || !output || !reflection) return;
    memset(reflection, 0, sizeof(*reflection));

    /* Simulated reflection: check output quality heuristics */
    size_t len = strlen(output);
    if (len < 20) {
        snprintf(reflection->reflection, sizeof(reflection->reflection),
                 "Output too short (%zu chars). Need more detail.", len);
        reflection->confidence = 0.3;
        reflection->should_retry = true;
    } else if (strstr(output, "I was unable") || strstr(output, "failed")) {
        snprintf(reflection->reflection, sizeof(reflection->reflection),
                 "Task may have failed. Try different tools or approach.");
        reflection->confidence = 0.4;
        reflection->should_retry = true;
    } else {
        snprintf(reflection->reflection, sizeof(reflection->reflection),
                 "Output looks reasonable. Confidence: high.");
        reflection->confidence = 0.85;
        reflection->should_retry = false;
    }
    reflection->retry_count = 0;
}

/* L8: Tree-of-Thought (ToT) search (Yao et al. 2023)
 * Instead of single CoT chain, explore multiple reasoning branches.
 * BFS over thought states with pruning based on self-evaluation.
 *
 * Algorithm:
 * 1. Generate k candidate thoughts from current state
 * 2. Evaluate each thought (value function)
 * 3. Keep top-b thoughts (beam search)
 * 4. Repeat until solution found or max depth */

void agent_tot_init(ToTState *state)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->best_value = -1.0;
}

/* Generate candidate thoughts (simulated) */
static void tot_generate_candidates(ToTState *state, uint32_t parent_idx,
                                     int k)
{
    const char *templates[] = {
        "Decompose the problem into sub-tasks and solve step by step.",
        "Search for relevant information using external tools.",
        "Analyze the problem from first principles.",
        "Consider edge cases and potential pitfalls.",
        "Formulate a hypothesis and verify with evidence."
    };
    int nt = (int)(sizeof(templates)/sizeof(templates[0]));

    for (int i = 0; i < k && state->count < 64; i++) {
        ToTNode *n = &state->nodes[state->count];
        snprintf(n->thought, sizeof(n->thought), "%s",
                 templates[(i + parent_idx) % nt]);
        /* Simulated value: random but favoring later templates */
        n->value = 0.3 + 0.7 * ((double)rand() / (double)RAND_MAX);
        n->depth = (parent_idx < (uint32_t)state->count) ?
                    state->nodes[parent_idx].depth + 1 : 0;
        n->parent_idx = parent_idx;
        if (n->value > state->best_value) {
            state->best_value = n->value;
            state->best_idx = state->count;
        }
        state->count++;
    }
}

void agent_tot_search(ToTState *state, const char *problem)
{
    if (!state || !problem) return;
    agent_tot_init(state);

    /* Root node */
    ToTNode *root = &state->nodes[0];
    snprintf(root->thought, sizeof(root->thought),
             "Problem: %s", problem);
    root->value = 1.0;
    root->depth = 0;
    root->parent_idx = 0;
    state->count = 1;
    state->best_idx = 0;
    state->best_value = 1.0;

    /* BFS expansion */
    for (uint32_t d = 0; d < TOT_MAX_DEPTH; d++) {
        uint32_t start = state->count;
        /* Generate candidates from nodes at this depth */
        uint32_t candidates = 0;
        for (uint32_t i = 0; i < state->count && candidates < TOT_MAX_BRANCHES * TOT_BEAM_WIDTH; i++) {
            if (state->nodes[i].depth == d) {
                tot_generate_candidates(state, i, TOT_MAX_BRANCHES);
                candidates++;
            }
        }
        /* Prune: keep only top beam_width candidates at this depth */
        if (state->count - start > TOT_BEAM_WIDTH) {
            /* Sort new nodes by value (selection sort, descending) */
            for (uint32_t i = start; i < state->count; i++) {
                uint32_t max_idx = i;
                for (uint32_t j = i + 1; j < state->count; j++) {
                    if (state->nodes[j].value > state->nodes[max_idx].value)
                        max_idx = j;
                }
                if (max_idx != i) {
                    ToTNode tmp = state->nodes[i];
                    state->nodes[i] = state->nodes[max_idx];
                    state->nodes[max_idx] = tmp;
                }
            }
            state->count = start + TOT_BEAM_WIDTH;
        }
    }
}

/* L9: Tool-Augmented Language Model (TALM) concepts
 * Parisi et al. 2022 "TALM: Tool Augmented Language Models"
 *
 * Key ideas:
 * 1. Tool descriptions as part of prompt engineering
 * 2. Tool outputs fed back as natural language observations
 * 3. Iterative refinement through tool use
 * 4. Self-play training for tool selection policy
 *
 * Google's LaMDA, Anthropic's Claude tool use, OpenAI function calling
 * all implement variations of the TALM paradigm.
 *
 * L4: No Free Lunch Theorem for tool selection —
 * No single tool selection strategy is optimal for all tasks.
 * The best strategy depends on the task distribution P(task).
 * Adaptive strategies that learn from feedback outperform
 * fixed heuristics asymptotically (Robbins-Monro 1951). */

/* Tool selection with UCB (Upper Confidence Bound) exploration
 * For multi-armed bandit tool selection:
 * UCB_i = mean_reward_i + sqrt(2*log(total_uses) / uses_i)
 * Balances exploitation (high mean reward) vs exploration (few uses) */
void agent_tool_stats_init(ToolStats *stats, uint32_t n)
{
    if (!stats) return;
    for (uint32_t i = 0; i < n; i++) {
        stats[i].uses = 0;
        stats[i].total_reward = 0.0;
        stats[i].mean_reward = 0.0;
        stats[i].ucb_score = 0.0;
    }
}

uint32_t agent_select_tool_ucb(const AgentRuntime *agent,
                                const char *query, ToolStats *stats)
{
    if (!agent || !query || !stats) return 0;
    uint32_t total = 0;
    for (uint32_t i = 0; i < agent->config.tool_count; i++) total += stats[i].uses;

    uint32_t best = 0;
    double best_score = -1.0;
    for (uint32_t i = 0; i < agent->config.tool_count; i++) {
        if (stats[i].uses == 0) {
            stats[i].ucb_score = INFINITY;
        } else {
            stats[i].ucb_score = stats[i].mean_reward +
                sqrt(2.0 * log((double)(total + 1)) / (double)stats[i].uses);
        }
        if (stats[i].ucb_score > best_score) {
            best_score = stats[i].ucb_score;
            best = i;
        }
    }
    /* Fall back to keyword matching if no exploration needed */
    uint32_t keyword_best = agent_select_tool(agent, query);
    if (keyword_best < agent->config.tool_count) {
        if ((double)rand() / (double)RAND_MAX < 0.1) return best;
    }
    return keyword_best;
}

void agent_tool_update_stats(ToolStats *stats, uint32_t tool_idx, double reward)
{
    if (!stats) return;
    stats[tool_idx].uses++;
    stats[tool_idx].total_reward += reward;
    stats[tool_idx].mean_reward = stats[tool_idx].total_reward / (double)stats[tool_idx].uses;
}

/* L9: Multi-Agent Debate and Consensus (Du et al. 2023)
 * Multiple agents independently reason about a problem, then
 * debate to reach consensus. Improves factuality and reduces
 * hallucination compared to single-agent approaches.
 *
 * Process:
 * 1. Each agent independently generates an answer
 * 2. Agents share answers and critique each other
 * 3. Iterative refinement: incorporate critiques
 * 4. Majority vote or weighted confidence to select final answer
 *
 * L4: Condorcet Jury Theorem — if each agent has independent
 * probability p > 0.5 of being correct, majority vote of N agents
 * approaches certainty as N → ∞.
 *
 * Reference: Du et al. 2023 "Improving Factuality and Reasoning
 * in Language Models through Multiagent Debate" (MIT/Google) */

void agent_debate_init(AgentDebate *debate)
{
    if (!debate) return;
    memset(debate, 0, sizeof(*debate));
}

void agent_debate_add_answer(AgentDebate *debate, const char *answer)
{
    if (!debate || !answer || debate->answer_count >= 8) return;
    strncpy(debate->answers[debate->answer_count], answer, 1023);
    debate->answer_count++;
}

/* Simple voting: count exact matches */
void agent_debate_vote(AgentDebate *debate)
{
    if (!debate) return;
    for (uint32_t i = 0; i < debate->answer_count; i++) {
        /* Self-vote = 1, plus votes from similar answers */
        debate->votes[i] = 1;
        for (uint32_t j = 0; j < debate->answer_count; j++) {
            if (i != j && strcmp(debate->answers[i], debate->answers[j]) == 0)
                debate->votes[i]++;
        }
    }
}

uint32_t agent_debate_winner(const AgentDebate *debate)
{
    if (!debate || debate->answer_count == 0) return 0;
    uint32_t best = 0, best_votes = 0;
    for (uint32_t i = 0; i < debate->answer_count; i++) {
        if (debate->votes[i] > best_votes) {
            best_votes = debate->votes[i]; best = i;
        }
    }
    return best;
}

/* L9: Agent Safety — Constitutional AI principles (Bai et al. 2022)
 *
 * Anthropic's Constitutional AI approach:
 * 1. Supervised phase: train model to follow a "constitution"
 *    of principles (e.g., "be helpful, harmless, honest")
 * 2. RL phase: model critiques and revises its own outputs
 *    based on constitutional principles
 *
 * Simulated safety checks for agent outputs:
 * - Harmfulness: reject outputs containing dangerous instructions
 * - Honesty: flag outputs with unsupported factual claims
 * - Privacy: detect and redact PII patterns */

SafetyLabel agent_safety_check(const char *output)
{
    if (!output) return SAFE;
    /* Simple keyword-based safety filter (production uses classifier models) */
    const char *harmful_kw[] = {"exploit", "malware", "bypass security", "illegal", NULL};
    const char *privacy_kw[] = {"credit card", "SSN", "password", "social security", NULL};
    for (const char **kw = harmful_kw; *kw; kw++)
        if (strstr(output, *kw)) return FLAG_HARMFUL;
    for (const char **kw = privacy_kw; *kw; kw++)
        if (strstr(output, *kw)) return FLAG_PRIVACY;
    return SAFE;
}

const char *agent_safety_label_str(SafetyLabel label)
{
    switch (label) {
    case SAFE:           return "SAFE";
    case FLAG_HARMFUL:   return "HARMFUL_CONTENT";
    case FLAG_DISHONEST: return "POTENTIALLY_INACCURATE";
    case FLAG_PRIVACY:   return "PRIVACY_CONCERN";
    default:             return "UNKNOWN";
    }
}