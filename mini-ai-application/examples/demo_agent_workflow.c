#include "agent_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void agent_register_default_tools(AgentRuntime *agent);
void agent_clear_memory(AgentRuntime *agent);
void agent_debate_init(AgentDebate *debate);
void agent_debate_add_answer(AgentDebate *debate, const char *answer);
void agent_debate_vote(AgentDebate *debate);
uint32_t agent_debate_winner(const AgentDebate *debate);

int main(void) {
    printf("\n============================================================\n");
    printf("  AI Agent Workflow Demo - ReAct Loop + Multi-Agent Debate\n");
    printf("============================================================\n\n");

    printf("[1] Creating AI Agent with ReAct capabilities...\n");
    AgentRuntime agent;
    agent_init(&agent);
    agent_set_prompt(&agent, "You are a helpful AI research assistant.");
    agent_register_default_tools(&agent);
    printf("    Agent initialized with %u tools\n", agent.config.tool_count);

    printf("\n[2] Running ReAct loop: 'Calculate 15 * 7 and explain the result'\n");
    agent_react_loop(&agent, "Calculate 15 * 7 and explain the result");
    printf("    Task completed: %s\n", agent.task_completed ? "YES" : "NO");
    printf("    Steps taken: %u\n", agent.steps_taken);
    printf("    Final answer: %s\n", agent.final_answer);

    printf("\n[3] Agent execution trace:\n");
    agent_print_trace(&agent);

    printf("\n[4] Multi-Agent Debate Demo:\n");
    AgentDebate debate;
    agent_debate_init(&debate);
    agent_debate_add_answer(&debate, "Reinforcement learning is best for sequential decisions");
    agent_debate_add_answer(&debate, "Supervised learning would be more appropriate here");
    agent_debate_add_answer(&debate, "A hybrid approach combining both methods is optimal");
    agent_debate_add_answer(&debate, "Reinforcement learning is best for sequential decisions");
    agent_debate_add_answer(&debate, "Supervised learning would be more appropriate here");
    printf("    %u agents submitted answers\n", debate.answer_count);
    agent_debate_vote(&debate);
    uint32_t winner = agent_debate_winner(&debate);
    printf("    Winner: Agent %u with %u votes\n", winner, debate.votes[winner]);
    printf("    Answer: '%s'\n", debate.answers[winner]);

    agent_clear_memory(&agent);
    printf("\n[5] Memory cleared. Demo complete.\n");
    return 0;
}
