#!/usr/bin/env python3
'''combine signal reports into one'''
import argparse, json, sys, glob

def combine(input_list,output_file):
    #report = {'reports':[]}
    reports = []
    # sources = []
    source_reports = dict()
    for i,fname in enumerate(input_list):
        # load json
        try:
            v = json.load(open(fname,'r'))
        except:
            continue

        # set extension
        ext = ':' + str(i)

        for r in v['reports']:
            r['instance_name'] += ext
            if r['report_type']=='energy':
                reports.append(r)
            if r['report_type']=='signal':
                r['energy_set'] = list(es+ext for es in r['energy_set'])
                reports.append(r)
            if r['report_type']=='source':
                # r['signal_set'] = list(ss+ext for ss in r['signal_set'])
                # if r['instance_name'] not in sources:
                #     sources.append(r['instance_name'])
                if r['device_origin'] not in source_reports:
                    print("Tracking new source:",r['device_origin'])
                    source_reports[r['device_origin']] = {'stats':r,'set':list()}
                source_reports[r['device_origin']]['set'] += list(ss+ext for ss in r['signal_set'])
    for do,dsr in source_reports.items():
        ss = dsr['set']
        r = dsr['stats']
        r['signal_set'] = ss
        reports.append(r)

    # export result
    with open(output_file,'w') as fid:
        json.dump({'reports':reports}, fid, indent=2)



if __name__ == '__main__':
    # options
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input', nargs='+', type=str,       help='input data files')
    p.add_argument('-output', type=str, default='report.json', help='output consolidated report file')
    args = p.parse_args()

    if len(args.input) == 1 and '*' in args.input[0]:
        args.input = glob.glob(args.input[0],recursive=True)
    print(args.input)
    combine(args.input,args.output)
    print('results written to', args.output)