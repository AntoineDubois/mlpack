/**
 * @file methods/reinforcement_learning/replay/random_replay.hpp
 * @author Shangtong Zhang
 *
 * This file is an implementation of random experience replay.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef MLPACK_METHODS_RL_REPLAY_RANDOM_REPLAY_HPP
#define MLPACK_METHODS_RL_REPLAY_RANDOM_REPLAY_HPP

#include <cassert>
#include <mlpack/prereqs.hpp>

namespace mlpack {

/**
 * Implementation of random experience replay.
 *
 * At each time step, interactions between the agent and the
 * environment will be saved to a memory buffer. When necessary,
 * we can simply sample previous experiences from the buffer to
 * train the agent. Typically this would be a random sample and
 * the memory will be a First-In-First-Out buffer.
 *
 * For more information, see the following.
 *
 * @code
 * @phdthesis{lin1993reinforcement,
 *  title  = {Reinforcement learning for robots using neural networks},
 *  author = {Lin, Long-Ji},
 *  year   = {1993},
 *  school = {Fujitsu Laboratories Ltd}
 * }
 * @endcode
 *
 * @tparam EnvironmentType Desired task.
 */
template <
  typename EnvironmentType,
  bool saveProba = false
> class RandomReplay
{
  public:
    //! Convenient typedef for action.
    using ActionType = typename EnvironmentType::Action;

    //! Convenient typedef for state.
    using StateType = typename EnvironmentType::State;

    //! Convenient typedef for reward
    using RewardType = double;

    struct Transition {
      StateType state;
      ActionType action;
      double reward;
      StateType nextState;
      bool isEnd;
    };

    RandomReplay() : batchSize(0), capacity(0), position(0),
          full(false), nSteps(0),
          totalReturn(0), episodeSteps(0)
    {
      /* Nothing to do here. */
    }

    /**
     * Construct an instance of random experience replay class.
     *
     * @param batchSize Number of examples returned at each sample.
     * @param capacity Total memory size in terms of number of examples.
     * @param nSteps Number of steps to look in the future.
     * @param backpropagate The replay method backpropagates the return at the end
     * of each episode to save the episode return.
     * @param dimension The dimension of an encoded state.
     */
    RandomReplay(const size_t batchSize, const size_t capacity,
                 const size_t nSteps = 1, const bool backpropagate = false,
                 const size_t dimension = StateType::dimension)
        : batchSize(batchSize), capacity(capacity), position(0), full(false),
          nSteps(nSteps), states(dimension, capacity), actions(capacity),
          rewards(capacity), nextStates(dimension, capacity),
          isTerminal(capacity), totalReturn(0), episodeSteps(0),
          backpropagate(backpropagate) { /* Nothing to do here. */ }

    /**
     * Store the given experience.
     *
     * @param state Given state.
     * @param action Given action.
     * @param reward Given reward.
     * @param nextState Given next state.
     * @param isEnd Whether next state is terminal state.
     * @param discount The discount parameter.
     */
    void Store(StateType state, ActionType action, double reward,
               StateType nextState, bool isEnd, const double &discount) {
      nStepBuffer.push_back({state, action, reward, nextState, isEnd});

      // Single step transition is not ready.
      if (nStepBuffer.size() < nSteps)
        return;

      // To keep the queue size fixed to nSteps.
      if (nStepBuffer.size() > nSteps)
        nStepBuffer.pop_front();

      // Before moving ahead, lets confirm if our fixed size buffer works.
      assert(nStepBuffer.size() == nSteps);

      // Make a n-step transition.
      GetNStepInfo(reward, nextState, isEnd, discount);

      state = nStepBuffer.front().state;
      action = nStepBuffer.front().action;

      states.col(position) = state.Encode();
      actions[position] = action;
      rewards(position) = reward;
      nextStates.col(position) = nextState.Encode();
      isTerminal(position) = isEnd;
      totalReturn += reward;
      position++;
      if (position == capacity) {
        full = true;
        position = 0;
      }
      episodeSteps++;
      if (isEnd && backpropagate) {
        Backward();
        episodeSteps = 0;
        totalReturn = 0.0;
      }
    }

    /**
     * Get the reward, next state and terminal boolean for nth step.
     *
     * @param reward Given reward.
     * @param nextState Given next state.
     * @param isEnd Whether next state is terminal state.
     * @param discount The discount parameter.
     */
    void GetNStepInfo(double &reward, StateType &nextState, bool &isEnd,
                      const double &discount) {
      reward = nStepBuffer.back().reward;
      nextState = nStepBuffer.back().nextState;
      isEnd = nStepBuffer.back().isEnd;

      // Should start from the second last transition in buffer.
      for (int i = nStepBuffer.size() - 2; i >= 0; i--) {
        bool iE = nStepBuffer[i].isEnd;
        reward = nStepBuffer[i].reward + discount * reward * (1 - iE);
        if (iE) {
          nextState = nStepBuffer[i].nextState;
          isEnd = iE;
        }
      }
    }

    /**
     * Sample some experiences.
     *
     * @param sampledStates Sampled encoded states.
     * @param sampledActions Sampled actions.
     * @param sampledRewards Sampled rewards.
     * @param sampledNextStates Sampled encoded next states.
     * @param isTerminal Indicate whether corresponding next state is terminal
     *        state.
     */
    void Sample(arma::mat &sampledStates,
                std::vector<ActionType> &sampledActions,
                arma::rowvec &sampledRewards,
                arma::mat &sampledNextStates,
                arma::irowvec &isTerminal)
    {
      size_t upperBound = full ? capacity : position;
      arma::uvec sampledIndices =
          randi<arma::uvec>(batchSize, arma::distr_param(0, upperBound - 1));

      sampledStates = states.cols(sampledIndices);
      for (size_t t = 0; t < sampledIndices.n_rows; t++)
        sampledActions.push_back(actions[sampledIndices[t]]);
      sampledRewards = rewards.elem(sampledIndices).t();
      sampledNextStates = nextStates.cols(sampledIndices);
      isTerminal = this->isTerminal.elem(sampledIndices).t();
    }

    /**
     * Get the number of transitions in the memory.
     *
     * @return Actual used memory size
     */
    const size_t &Size() { return full ? capacity : position; }

    /**
     * Update the priorities of transitions and Update the gradients.
     *
     * @param * (target) The learned value
     * @param * (sampledActions) Agent's sampled action
     * @param * (nextActionValues) Agent's next action
     * @param * (gradients) The model's gradients
     */
    void Update(arma::mat /* target */,
                std::vector<ActionType> /* sampledActions */,
                arma::mat /* nextActionValues */, arma::mat & /* gradients */)
    {
      /* Do nothing for random replay. */
    }

    //! Get the number of steps for n-step agent.
    const size_t &NSteps() const { return nSteps; }

    /**
     * Backpropagate the total return
     * Used in AlphaZero, and in MC approaches
     *
     * @param * (totalReturn) The total return over the episode
     * @param * (Steps) On how many steps to backpropagate the total return
     */
    void Backward() {
      for (size_t step = 0, i; step < episodeSteps; ++step) {
        i = (position - 1 - step + capacity) % capacity;
        rewards(i) = totalReturn;
      }
    }

  private:
    //! Locally-stored number of examples of each sample.
    size_t batchSize;

    //! Locally-stored total memory limit.
    size_t capacity;

    //! Indicate the position to store new transition.
    size_t position;

    //! Locally-stored indicator that whether the memory is full or not
    bool full;

    //! Locally-stored number of steps to look into the future.
    size_t nSteps;

    //! Locally-stored whether or not to backpropagate the reward
    bool backpropagate;

    //! Locally-stored number of steps of an episode.
    size_t episodeSteps;

    //! Locally-stored the total reward of an episode.
    RewardType totalReturn;

    //! Locally-stored buffer containing n consecutive steps.
    std::deque<Transition> nStepBuffer;

    //! Locally-stored encoded previous states.
    arma::mat states;

    //! Locally-stored previous actions.
    std::vector<ActionType> actions;

    //! Locally-stored previous rewards.
    arma::rowvec rewards;

    //! Locally-stored encoded previous next states.
    arma::mat nextStates;

    //! Locally-stored termination information of previous experience.
    arma::irowvec isTerminal;
};

template <
  typename EnvironmentType
> class RandomReplay<EnvironmentType, true>
{
  public:
    //! Convenient typedef for action.
    using ActionType = typename EnvironmentType::Action;

    //! Convenient typedef for state.
    using StateType = typename EnvironmentType::State;

    //! Convenient typedef for reward
    using RewardType = double;

    struct Transition {
      StateType state;
      arma::vec probaAction;
      double reward;
      StateType nextState;
      bool isEnd;
    };

    RandomReplay() :
      batchSize(0), capacity(0),
      position(0), full(false),
      nSteps(0)
    {
      /* Nothing to do here. */
    }

    /**
     * Construct an instance of random experience replay class.
     *
     * @param batchSize Number of examples returned at each sample.
     * @param capacity Total memory size in terms of number of examples.
     * @param nSteps Number of steps to look in the future.
     * @param backpropagate The replay method backpropagates the return at the end
     * of each episode to save the episode return.
     * @param dimension The dimension of an encoded state.
     */
    RandomReplay(const size_t batchSize, const size_t capacity,
                 const size_t nSteps = 1, const bool backpropagate = false,
                 const size_t dimension = StateType::dimension)
        : batchSize(batchSize), capacity(capacity), position(0), full(false),
          nSteps(nSteps), states(dimension, capacity),
          probaActions(ActionType::size, capacity), rewards(capacity),
          nextStates(dimension, capacity), isTerminal(capacity), totalReturn(0),
          episodeSteps(0),
          backpropagate(backpropagate) { /* Nothing to do here. */ }

    /**
     * Store the given experience.
     *
     * @param state Given state.
     * @param probaAction Given action’s probabilities.
     * @param reward Given reward.
     * @param nextState Given next state.
     * @param isEnd Whether next state is terminal state.
     * @param discount The discount parameter.
     */
    void Store(StateType state, arma::vec probaAction, double reward,
               StateType nextState, bool isEnd, const double &discount)
    {
      nStepBuffer.push_back({state, probaAction, reward, nextState, isEnd});

      // Single step transition is not ready.
      if (nStepBuffer.size() < nSteps)
        return;

      // To keep the queue size fixed to nSteps.
      if (nStepBuffer.size() > nSteps)
        nStepBuffer.pop_front();

      // Before moving ahead, lets confirm if our fixed size buffer works.
      assert(nStepBuffer.size() == nSteps);

      // Make a n-step transition.
      GetNStepInfo(reward, nextState, isEnd, discount);

      state = nStepBuffer.front().state;
      probaAction = nStepBuffer.front().probaAction;

      states.col(position) = state.Encode();
      probaActions.col(position) = probaAction;
      rewards(position) = reward;
      nextStates.col(position) = nextState.Encode();
      isTerminal(position) = isEnd;
      totalReturn += reward;
      position++;
      if (position == capacity) {
        full = true;
        position = 0;
      }
      episodeSteps++;
      if (isEnd && backpropagate) {
        Backward();
        episodeSteps = 0;
        totalReturn = 0.0;
      }
    }

    /**
     * Get the reward, next state and terminal boolean for nth step.
     *
     * @param reward Given reward.
     * @param nextState Given next state.
     * @param isEnd Whether next state is terminal state.
     * @param discount The discount parameter.
     */
    void GetNStepInfo(double &reward, StateType &nextState, bool &isEnd,
                      const double &discount)
    {
      reward = nStepBuffer.back().reward;
      nextState = nStepBuffer.back().nextState;
      isEnd = nStepBuffer.back().isEnd;

      // Should start from the second last transition in buffer.
      for (int i = nStepBuffer.size() - 2; i >= 0; i--) {
        bool iE = nStepBuffer[i].isEnd;
        reward = nStepBuffer[i].reward + discount * reward * (1 - iE);
        if (iE) {
          nextState = nStepBuffer[i].nextState;
          isEnd = iE;
        }
      }
    }

    /**
     * Sample some experiences.
     *
     * @param sampledStates Sampled encoded states.
     * @param sampledProbaActions Sampled actions’ probabilities.
     * @param sampledRewards Sampled rewards.
     * @param sampledNextStates Sampled encoded next states.
     * @param isTerminal Indicate whether corresponding next state is terminal
     *        state.
     */
    void Sample(arma::mat &sampledStates,
                arma::mat &sampledProbaActions,
                arma::rowvec &sampledRewards,
                arma::mat &sampledNextStates,
                arma::irowvec &isTerminal)
    {
      size_t upperBound = full ? capacity : position;
      arma::uvec sampledIndices =
          randi<arma::uvec>(batchSize, arma::distr_param(0, upperBound - 1));

      sampledStates = states.cols(sampledIndices);
      sampledProbaActions = probaActions.cols(sampledIndices);
      sampledRewards = rewards.elem(sampledIndices).t();
      sampledNextStates = nextStates.cols(sampledIndices);
      isTerminal = this->isTerminal.elem(sampledIndices).t();
    }

    /**
     * Get the number of transitions in the memory.
     *
     * @return Actual used memory size
     */
    const size_t &Size() { return full ? capacity : position; }

    /**
     * Update the priorities of transitions and Update the gradients.
     *
     * @param * (target) The learned value
     * @param * (sampledProbaActions) Agent's sampled probability actions
     * @param * (nextActionValues) Agent's next action
     * @param * (gradients) The model's gradients
     */
    void Update(arma::mat /* target */, arma::mat /* sampledProbaActions */,
                arma::mat /* nextActionValues */, arma::mat & /* gradients */) {
      /* Do nothing for random replay. */
    }

    //! Get the number of steps for n-step agent.
    const size_t &NSteps() const { return nSteps; }

    /**
     * Backpropagate the total return
     * Used in AlphaZero
     *
     * @param * (totalReturn) The total return over the episode
     * @param * (Steps) On how many steps to backpropagate the total return
     */
    void Backward() {
      for (size_t step = 0, i; step < episodeSteps; ++step) {
        i = (position - 1 - step + capacity) % capacity;
        rewards(i) = totalReturn;
      }
    }

  private:
    //! Locally-stored number of examples of each sample.
    size_t batchSize;

    //! Locally-stored total memory limit.
    size_t capacity;

    //! Indicate the position to store new transition.
    size_t position;

    //! Locally-stored indicator that whether the memory is full or not
    bool full;

    //! Locally-stored number of steps to look into the future.
    size_t nSteps;

    //! Locally-stored whether or not to backpropagate the reward
    bool backpropagate;

    //! Locally-stored number of steps of an episode.
    size_t episodeSteps;

    //! Locally-stored the total reward of an episode.
    RewardType totalReturn;

    //! Locally-stored buffer containing n consecutive steps.
    std::deque<Transition> nStepBuffer;

    //! Locally-stored encoded previous states.
    arma::mat states;

    //! Locally-stored previous actions.
    arma::mat probaActions;

    //! Locally-stored previous rewards.
    arma::rowvec rewards;

    //! Locally-stored encoded previous next states.
    arma::mat nextStates;

    //! Locally-stored termination information of previous experience.
    arma::irowvec isTerminal;
};

} // namespace mlpack

#endif

