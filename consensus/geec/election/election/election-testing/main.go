package main

// #cgo pkgconfig: dl sgx_urts

import (
	"github.com/ethereum/go-ethereum/consensus/trustedHW/election"
	"strconv"
	"sync"
)

func main(){

	groupsize := 50
	start := uint64(100)
	leng := uint64(5000)


	param := new(election.GroupParams)

	param.Ipstrs = make([]string, groupsize)
	param.Ports = make([]int, groupsize)
	accounts  := make([]string, groupsize)

	for i:= 0; i<groupsize; i++{
		param.Ipstrs[i] = "127.0.0.1"
		param.Ports[i] = 20000+i
		accounts[i] = strconv.Itoa(i)
	}

	param.CommitteeCount = groupsize
	param.Start_blk = start
	param.Term_len = leng





	gs := make([]*election.PaxosGroup, groupsize)
	for i:= 0; i< groupsize; i++{
		param.Account = accounts[i]
		param.Offset = i
		gs[i] = election.NewGroup(param)
	}
	
	wg := new(sync.WaitGroup)

	wg.Add(groupsize)
	for j:= 0; j < groupsize; j++{
		go func(j int, wg *sync.WaitGroup){
			for i := start; i < leng; i++ {
				gs[j].Elect(uint64(i))
			}
			wg.Done()
		}(j, wg)
	}
	wg.Wait()


	//go func(){

	//}()


	//time.Sleep(10* time.Second);
	//param.Offset = 1
	//gs[1] = election.NewGroup(param)
	//
	////time.Sleep(10* time.Second);
	//param.Offset = 2
	//gs[2] = election.NewGroup(param)

	//for i:= 100; i < 200; i++ {
	//	for j:= 0; j < 3; j++{
	//		gs[j].Elect(uint64(i));
	//	}
	//}
	//
	//for i:= 0; i< groupsize; i++{
	//	gs[i].DestroyGroup()
	//}
}