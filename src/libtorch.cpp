#include <libtorch.h>
// #include <ATen/cuda/CUDAContext.h>
// #include <ATen/cuda/CUDAGuard.h>

#include <iostream>

NeuralNetwork::NeuralNetwork(std::string model_path, bool use_gpu)
    : module(torch::jit::load(model_path.c_str())), use_gpu(use_gpu) {
  if (this->use_gpu) {
    // move to CUDA
    this->module->to(at::kCUDA);
  }
  assert(this->module != nullptr);
}

std::vector<std::vector<double>> NeuralNetwork::infer(Gomoku* gomoku) {
  int n = gomoku->get_n();

  // convert data format
  auto board = gomoku->get_board();
  std::vector<int> board0;
  for (unsigned int i = 0; i < board.size(); i++) {
    board0.insert(board0.end(), board[i].begin(), board[i].end());
  }

  torch::Tensor temp =
      torch::from_blob(&board0[0], {1, 1, n, n}, torch::dtype(torch::kInt32));

  torch::Tensor state0 = temp.gt(0).toType(torch::kFloat32);
  torch::Tensor state1 = temp.lt(0).toType(torch::kFloat32);

  int last_move = gomoku->get_last_move();
  int cur_player = gomoku->get_current_color();

  if (cur_player == -1) {
    std::swap(state0, state1);
  }

  torch::Tensor state2 =
      torch::zeros({1, 1, n, n}, torch::dtype(torch::kFloat32));

  if (last_move != -1) {
    state2[0][0][last_move / n][last_move % n] = 1;
  }
  torch::Tensor state3 =
      torch::ones({1, 1, n, n}, torch::dtype(torch::kFloat32));
  state3 *= cur_player;

  // infer
  torch::Tensor states = torch::cat({state0, state1, state2, state3}, 1);

  if (this->use_gpu) {
    // use different CUDA stream
    // https://github.com/pytorch/pytorch/issues/16614
    // at::cuda::CUDAStream stream = at::cuda::getStreamFromPool();
    // at::cuda::CUDAStreamGuard guard(stream);

    states = states.to(at::kCUDA);

    std::vector<torch::jit::IValue> inputs{states};
    auto result = this->module->forward(inputs).toTuple();

    torch::Tensor p =
        result->elements()[0].toTensor().exp().toType(torch::kFloat32)[0];
    torch::Tensor v =
        result->elements()[1].toTensor().toType(torch::kFloat32)[0];

    p = p.to(at::kCPU);
    v = v.to(at::kCPU);

    // output
    std::vector<double> prob(static_cast<float*>(p.data_ptr()),
                             static_cast<float*>(p.data_ptr()) + n * n);
    std::vector<double> value{v.item<float>()};
    return {prob, value};

  } else {
    std::vector<torch::jit::IValue> inputs{states};
    auto result = this->module->forward(inputs).toTuple();

    torch::Tensor p =
        result->elements()[0].toTensor().exp().toType(torch::kFloat32)[0];
    torch::Tensor v =
        result->elements()[1].toTensor().toType(torch::kFloat32)[0];

    // output
    std::vector<double> prob(static_cast<float*>(p.data_ptr()),
                             static_cast<float*>(p.data_ptr()) + n * n);
    std::vector<double> value{v.item<float>()};
    return {prob, value};
  }
}
